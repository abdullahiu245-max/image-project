// EZW Image Compression — backend API
//
// Wraps the compiled C++ EZW binary (ezw_compression.cpp) as an
// HTTP service. Receives an uploaded image, shells out to the
// binary in "--api" mode, then returns the compression stats
// plus the reconstructed image (base64) to the caller.
//
// This is meant to be deployed somewhere that can run an
// arbitrary Linux binary — e.g. Render, Railway, Fly.io, a VPS —
// NOT Vercel (Vercel's serverless functions can't run a compiled
// C++ binary + OpenCV like this needs). The frontend is the part
// that goes on Vercel; see ../frontend.

const express = require("express");
const cors = require("cors");
const multer = require("multer");
const { execFile } = require("child_process");
const fs = require("fs");
const path = require("path");
const os = require("os");
const { randomUUID } = require("crypto");

const app = express();

const PORT = process.env.PORT || 8080;
const EZW_BINARY = process.env.EZW_BINARY_PATH || path.join(__dirname, "ezw");
const MAX_UPLOAD_BYTES = 15 * 1024 * 1024; // 15 MB
const EXEC_TIMEOUT_MS = 30_000;

// Allow the frontend's origin. Set FRONTEND_ORIGIN in production
// (e.g. https://your-app.vercel.app) — defaults to "*" for local dev.
app.use(
  cors({
    origin: process.env.FRONTEND_ORIGIN || "*",
  })
);

const upload = multer({
  storage: multer.memoryStorage(),
  limits: { fileSize: MAX_UPLOAD_BYTES },
  fileFilter: (req, file, cb) => {
    const allowed = [".png", ".jpg", ".jpeg", ".pgm"];
    const ext = path.extname(file.originalname).toLowerCase();
    if (!allowed.includes(ext)) {
      return cb(new Error(`Unsupported file type: ${ext}`));
    }
    cb(null, true);
  },
});

app.get("/health", (req, res) => {
  res.json({ status: "ok" });
});

app.post("/compress", upload.single("image"), async (req, res) => {
  if (!req.file) {
    return res.status(400).json({ success: false, error: "No image uploaded (field name must be 'image')." });
  }

  const jobId = randomUUID();
  const tmpDir = os.tmpdir();
  const ext = path.extname(req.file.originalname).toLowerCase() || ".png";

  const inputPath = path.join(tmpDir, `${jobId}-input${ext}`);
  const compressedPath = path.join(tmpDir, `${jobId}-compressed.ezw`);
  // Output as PNG so the reconstructed image can be sent back as
  // a normal web image regardless of what the input format was.
  const outputPath = path.join(tmpDir, `${jobId}-output.png`);

  const passes = clampPasses(req.body.passes);

  try {
    fs.writeFileSync(inputPath, req.file.buffer);

    const stats = await runEzwBinary(inputPath, compressedPath, outputPath, passes);

    if (!stats.success) {
      return res.status(500).json({ success: false, error: stats.error || "Compression failed." });
    }

    const reconstructedBuffer = fs.readFileSync(outputPath);

    res.json({
      success: true,
      stats: {
        originalWidth: stats.origWidth,
        originalHeight: stats.origHeight,
        paddedWidth: stats.paddedWidth,
        paddedHeight: stats.paddedHeight,
        wavelettLevels: stats.levels,
        passes: stats.passes,
        originalFileSizeBytes: stats.originalFileSize,
        compressedFileSizeBytes: stats.compressedFileSize,
        compressionRatio: stats.compressionRatio,
        spaceSavingPercent: stats.spaceSavingPercent,
        mse: stats.mse,
        psnr: stats.psnr,
      },
      reconstructedImageBase64: `data:image/png;base64,${reconstructedBuffer.toString("base64")}`,
    });
  } catch (err) {
    console.error("Compression error:", err);
    res.status(500).json({ success: false, error: String(err.message || err) });
  } finally {
    // Best-effort cleanup — don't let a stray temp file block the response.
    for (const p of [inputPath, compressedPath, outputPath]) {
      fs.unlink(p, () => {});
    }
  }
});

function clampPasses(value) {
  const n = parseInt(value, 10);
  if (Number.isNaN(n)) return 12;
  return Math.min(Math.max(n, 4), 16);
}

function runEzwBinary(inputPath, compressedPath, outputPath, passes) {
  return new Promise((resolve, reject) => {
    execFile(
      EZW_BINARY,
      ["--api", inputPath, compressedPath, outputPath, String(passes)],
      { timeout: EXEC_TIMEOUT_MS, maxBuffer: 1024 * 1024 },
      (error, stdout, stderr) => {
        // Our --api mode always prints one line of JSON to stdout,
        // on both the success and failure path, so try to parse it
        // FIRST regardless of whether `error` is set (a non-zero
        // exit code alone still counts as `error` here, but the
        // JSON telling us *why* is still sitting in stdout).
        if (stdout && stdout.trim().startsWith("{")) {
          try {
            const parsed = JSON.parse(stdout.trim());
            return resolve(parsed);
          } catch (parseErr) {
            // fall through to the diagnostic branch below
          }
        }

        // If we get here, the binary either never ran, or crashed
        // before/without printing valid JSON (e.g. a C++-level
        // crash such as a segfault, missing shared library, or an
        // unhandled exception). Surface everything we know —
        // stderr in particular is where a crash reason or dynamic
        // linker error would show up.
        const details = {
          binaryPath: EZW_BINARY,
          exitCode: error ? error.code : null,
          signal: error ? error.signal : null,
          nodeErrorMessage: error ? error.message : null,
          stdout: stdout || "(empty)",
          stderr: stderr || "(empty)",
        };

        console.error("ezw binary did not produce usable output:", details);

        return reject(
          new Error(
            `Compression binary failed. ` +
              `signal=${details.signal} exitCode=${details.exitCode} ` +
              `stderr="${details.stderr}" stdout="${details.stdout}"`
          )
        );
      }
    );
  });
}

// Multer error handler (file too large, bad type, etc.)
app.use((err, req, res, next) => {
  if (err) {
    return res.status(400).json({ success: false, error: err.message });
  }
  next();
});

app.listen(PORT, () => {
  console.log(`EZW backend listening on port ${PORT}`);
  console.log(`Using binary: ${EZW_BINARY}`);

  try {
    fs.accessSync(EZW_BINARY, fs.constants.X_OK);
    console.log(`Binary check OK: ${EZW_BINARY} exists and is executable.`);
  } catch (err) {
    console.error(
      `WARNING: binary at ${EZW_BINARY} is missing or not executable (${err.code}). ` +
        `/compress will fail until this is fixed.`
    );
  }
});
