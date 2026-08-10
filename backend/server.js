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
