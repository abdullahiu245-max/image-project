"use client";

import { useState, useRef } from "react";

const API_URL = process.env.NEXT_PUBLIC_API_URL || "http://localhost:8080";

interface CompressionStats {
  uploadWidth: number;
  uploadHeight: number;
  originalWidth: number;
  originalHeight: number;
  paddedWidth: number;
  paddedHeight: number;
  wavelettLevels: number;
  passes: number;
  originalFileSizeBytes: number;
  compressedFileSizeBytes: number;
  compressionRatio: number;
  spaceSavingPercent: number;
  mse: number;
  psnr: number;
}

interface CompressResponse {
  success: boolean;
  error?: string;
  stats?: CompressionStats;
  reconstructedImageBase64?: string;
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}

export default function Home() {
  const [file, setFile] = useState<File | null>(null);
  const [originalPreview, setOriginalPreview] = useState<string | null>(null);
  const [passes, setPasses] = useState(12);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<CompressResponse | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  function handleFileChange(e: React.ChangeEvent<HTMLInputElement>) {
    const selected = e.target.files?.[0] ?? null;
    setFile(selected);
    setResult(null);
    setError(null);

    if (selected) {
      const reader = new FileReader();
      reader.onload = () => setOriginalPreview(reader.result as string);
      reader.readAsDataURL(selected);
    } else {
      setOriginalPreview(null);
    }
  }

  async function handleCompress() {
    if (!file) return;

    setLoading(true);
    setError(null);
    setResult(null);

    try {
      const formData = new FormData();
      formData.append("image", file);
      formData.append("passes", String(passes));

      const res = await fetch(`${API_URL}/compress`, {
        method: "POST",
        body: formData,
      });

      const data: CompressResponse = await res.json();

      if (!res.ok || !data.success) {
        setError(data.error || "Compression failed.");
      } else {
        setResult(data);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : "Network error contacting the backend.");
    } finally {
      setLoading(false);
    }
  }

  return (
    <main className="min-h-screen px-6 py-12 max-w-5xl mx-auto">
      <header className="mb-10">
        <h1 className="text-3xl font-semibold tracking-tight">
          EZW Image Compression
        </h1>
        <p className="text-slate-400 mt-2">
          Embedded Zerotree Wavelet compression, running against a C++ backend.
          Upload an image to compress it and see the reconstructed result.
        </p>
      </header>

      <section className="bg-slate-900/60 border border-slate-800 rounded-xl p-6 mb-8">
        <div className="flex flex-col sm:flex-row gap-4 items-start sm:items-end">
          <div className="flex-1 w-full">
            <label className="block text-sm text-slate-400 mb-2">
              Image (JPG, PNG, or PGM)
            </label>
            <input
              ref={fileInputRef}
              type="file"
              accept=".jpg,.jpeg,.png,.pgm"
              onChange={handleFileChange}
              className="block w-full text-sm text-slate-300 file:mr-4 file:py-2 file:px-4 file:rounded-lg file:border-0 file:bg-indigo-600 file:text-white file:cursor-pointer hover:file:bg-indigo-500 cursor-pointer"
            />
          </div>

          <div className="w-full sm:w-40">
            <label className="block text-sm text-slate-400 mb-2">
              Passes ({passes})
            </label>
            <input
              type="range"
              min={4}
              max={16}
              value={passes}
              onChange={(e) => setPasses(Number(e.target.value))}
              className="w-full"
            />
          </div>

          <button
            onClick={handleCompress}
            disabled={!file || loading}
            className="w-full sm:w-auto px-6 py-2 rounded-lg bg-indigo-600 hover:bg-indigo-500 disabled:bg-slate-700 disabled:cursor-not-allowed font-medium transition-colors"
          >
            {loading ? "Compressing..." : "Compress"}
          </button>
        </div>

        {error && (
          <p className="mt-4 text-sm text-red-400 bg-red-950/40 border border-red-900 rounded-lg px-4 py-3">
            {error}
          </p>
        )}
      </section>

      {(originalPreview || result) && (
        <section className="grid sm:grid-cols-2 gap-6 mb-8">
          <div>
            <h2 className="text-sm text-slate-400 mb-2">Original</h2>
            <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-3 flex items-center justify-center min-h-[200px]">
              {originalPreview ? (
                // eslint-disable-next-line @next/next/no-img-element
                <img
                  src={originalPreview}
                  alt="Original upload"
                  className="max-w-full max-h-[400px] rounded-lg"
                />
              ) : (
                <span className="text-slate-600 text-sm">No image yet</span>
              )}
            </div>
          </div>

          <div>
            <h2 className="text-sm text-slate-400 mb-2">Reconstructed</h2>
            <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-3 flex items-center justify-center min-h-[200px]">
              {result?.reconstructedImageBase64 ? (
                // eslint-disable-next-line @next/next/no-img-element
                <img
                  src={result.reconstructedImageBase64}
                  alt="Reconstructed"
                  className="max-w-full max-h-[400px] rounded-lg"
                />
              ) : (
                <span className="text-slate-600 text-sm">
                  {loading ? "Working on it..." : "Compress an image to see this"}
                </span>
              )}
            </div>
          </div>
        </section>
      )}

      {result?.stats && (
        <section className="bg-slate-900/60 border border-slate-800 rounded-xl p-6">
          <h2 className="text-sm text-slate-400 mb-4">Results</h2>

          {(result.stats.uploadWidth !== result.stats.originalWidth ||
            result.stats.uploadHeight !== result.stats.originalHeight) && (
            <p className="text-xs text-amber-400/90 bg-amber-950/30 border border-amber-900/50 rounded-lg px-3 py-2 mb-4">
              Downscaled from {result.stats.uploadWidth} × {result.stats.uploadHeight} to{" "}
              {result.stats.originalWidth} × {result.stats.originalHeight} before compression, to keep
              memory use bounded on the server.
            </p>
          )}

          <div className="grid grid-cols-2 sm:grid-cols-3 gap-x-6 gap-y-4 text-sm">
            <Stat label="Dimensions" value={`${result.stats.originalWidth} × ${result.stats.originalHeight}`} />
            <Stat label="Wavelet levels" value={String(result.stats.wavelettLevels)} />
            <Stat label="Passes" value={String(result.stats.passes)} />
            <Stat label="Original size" value={formatBytes(result.stats.originalFileSizeBytes)} />
            <Stat label="Compressed size" value={formatBytes(result.stats.compressedFileSizeBytes)} />
            <Stat label="Compression ratio" value={`${result.stats.compressionRatio.toFixed(2)}:1`} />
            <Stat label="Space saving" value={`${result.stats.spaceSavingPercent.toFixed(1)}%`} />
            <Stat label="MSE" value={result.stats.mse.toFixed(4)} />
            <Stat label="PSNR" value={`${result.stats.psnr.toFixed(2)} dB`} />
          </div>
        </section>
      )}

      <footer className="mt-12 text-xs text-slate-600">
        Backend: <code>{API_URL}</code>
      </footer>
    </main>
  );
}

function Stat({ label, value }: { label: string; value: string }) {
  return (
    <div>
      <div className="text-slate-500">{label}</div>
      <div className="text-lg font-medium">{value}</div>
    </div>
  );
}
