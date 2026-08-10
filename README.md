# EZW Image Compression — web system

A working web version of the C++ EZW (Embedded Zerotree Wavelet)
image compression project: Haar wavelet transform + EZW zerotree
encoding, wrapped in a small HTTP API, fronted by a web UI.

```
                     ┌──────────────────────┐
   Browser  ───────▶ │  Next.js frontend     │  deployed on Vercel
   (upload image)    │  (repo root)          │  (zero config — it's
                     └──────────┬───────────┘   already the root)
                                │ POST /compress
                                ▼
                     ┌──────────────────────┐
                     │  Express API         │  deployed on Render /
                     │  (backend/server.js) │  Railway / Fly / a VPS
                     └──────────┬───────────┘  (Root Directory: backend)
                                │ spawns subprocess
                                ▼
                     ┌──────────────────────┐
                     │  ezw (C++ binary)     │
                     │  ezw_compression.cpp  │
                     │  + OpenCV for JPG/PNG │
                     └──────────────────────┘
```

## Why this layout

The Next.js app lives at the **repo root** on purpose — Vercel's
default "Root Directory" is the repo root, so importing this repo
into Vercel needs **zero manual configuration**. No path to type
in, nothing to get wrong.

The C++ engine + its Express wrapper live in `backend/`, one clean
subfolder — when you set up the backend on Render/Railway you'll
type exactly `backend` as the Root Directory there, nothing nested
deeper than that.

**Why two deployments at all?** Vercel's serverless functions can't
run an arbitrary compiled C++ binary with OpenCV linked against
it — Vercel is built for JS/static hosting, not general Linux
binaries. So the C++ binary runs behind a small Node API deployed
somewhere that *does* run normal containers (Render, Railway,
Fly.io, a VPS), and Vercel only hosts the frontend that talks to
it over HTTP.

## Folder structure

```
image-project/                (repo root = the Next.js app itself)
├── app/
│   ├── page.tsx               upload UI + results
│   ├── layout.tsx
│   └── globals.css
├── package.json
├── next.config.js
├── tailwind.config.js
├── tsconfig.json
├── .env.local.example
├── backend/                    Express API + the C++ engine
│   ├── ezw_compression.cpp
│   ├── server.js
│   ├── package.json
│   ├── Dockerfile
│   └── README.md               backend-specific deploy steps
└── README.md                   this file
```

## Quick start (local)

Terminal 1 — backend:
```bash
cd backend
docker build -t ezw-backend .
docker run -p 8080:8080 ezw-backend
```
(or without Docker: `npm install && npm run build:cpp && npm start`
— you'll need g++ and OpenCV installed locally for that)

Terminal 2 — frontend (from the repo root):
```bash
npm install
cp .env.local.example .env.local
npm run dev
```

Open http://localhost:3000, upload an image, hit Compress.

## Putting it on GitHub

```bash
cd image-project
git init
git add .
git commit -m "EZW image compression web system"
git branch -M main
git remote add origin https://github.com/<your-username>/<repo-name>.git
git push -u origin main
```

## Deploying for real

1. **Backend first** (frontend needs its URL): follow
   `backend/README.md` to deploy to Render or Railway, setting
   **Root Directory** to `backend`. You'll end up with a URL like
   `https://ezw-backend.onrender.com`.
2. **Frontend**: in Vercel, New Project → import this repo. Leave
   **Root Directory as the default (blank/repo root)** — don't set
   it to anything, that's the whole point of this layout. Vercel
   auto-detects Next.js. Add an environment variable
   `NEXT_PUBLIC_API_URL` = your backend URL from step 1, then
   deploy.
3. Go back to the backend's environment variables and set
   `FRONTEND_ORIGIN` to your Vercel URL, so CORS only allows your
   own frontend to call it. Redeploy the backend.

## A couple of things worth knowing before you demo this

- **Cold starts**: free tiers on Render/Railway spin the container
  down when idle, so the first request after a while can take
  10–30 seconds.
- **Image size limits**: capped at 15 MB uploads and a 30 second
  compression timeout in `backend/server.js` — turn these up if you
  need to.
- **Passes slider**: more passes = better reconstruction quality
  and a slightly larger compressed file, at the cost of more
  compute time per request. 12 is a reasonable default.
