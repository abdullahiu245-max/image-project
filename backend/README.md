# EZW backend

An HTTP API that wraps the C++ EZW compression binary. Deploy this
somewhere that runs Docker containers — **not Vercel**. Render and
Railway both have straightforward free/low-cost tiers for this.

## API

### `POST /compress`

Multipart form upload:
- `image` — the image file (`.png`, `.jpg`, `.jpeg`, or `.pgm`)
- `passes` — optional, integer 4–16 (default 12). Higher = better
  quality, larger compressed size, slower.

Response:

```json
{
  "success": true,
  "stats": {
    "originalWidth": 800,
    "originalHeight": 600,
    "paddedWidth": 1024,
    "paddedHeight": 1024,
    "wavelettLevels": 8,
    "passes": 12,
    "originalFileSizeBytes": 480000,
    "compressedFileSizeBytes": 61000,
    "compressionRatio": 7.87,
    "spaceSavingPercent": 87.29,
    "mse": 12.4,
    "psnr": 37.2
  },
  "reconstructedImageBase64": "data:image/png;base64,...."
}
```

### `GET /health`

Returns `{ "status": "ok" }`. Use this as the health check endpoint
on whichever platform you deploy to.

## Local development

You need g++ and (for JPG/PNG) OpenCV installed locally — see the
main C++ file's header comment for build commands, or just use
Docker:

```bash
docker build -t ezw-backend .
docker run -p 8080:8080 ezw-backend
```

Or without Docker, if you already have g++ + OpenCV set up:

```bash
npm install
npm run build:cpp          # compiles ./ezw with OpenCV support
npm start
```

Then test it:

```bash
curl -F "image=@/path/to/photo.jpg" -F "passes=12" http://localhost:8080/compress
```

## Deploying to Render

1. Push this repo to GitHub.
2. In Render: New → Web Service → connect the repo → set the
   **Root Directory** to `backend`.
3. Render will detect the `Dockerfile` automatically — pick
   "Docker" as the environment.
4. Set an environment variable `FRONTEND_ORIGIN` to your deployed
   Vercel URL (e.g. `https://your-app.vercel.app`) once you have it,
   so CORS only allows your frontend.
5. Deploy. Render gives you a URL like
   `https://ezw-backend.onrender.com` — that's what the frontend's
   `NEXT_PUBLIC_API_URL` should point to.

## Deploying to Railway

Same idea: New Project → Deploy from GitHub repo → set root
directory to `backend` → Railway auto-detects the Dockerfile →
set `FRONTEND_ORIGIN` env var → deploy.

## Notes

- Uploads are capped at 15 MB and each compression run has a 30
  second timeout — adjust `MAX_UPLOAD_BYTES` / `EXEC_TIMEOUT_MS` in
  `server.js` if you need different limits.
- The reconstructed image is always returned as PNG regardless of
  the input format, since that's a safe universal format for the
  browser to display.
- Files are written to the OS temp directory per-request with a
  random UUID and cleaned up after the response — nothing is kept
  on disk.
