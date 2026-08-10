# Builds the C++ EZW binary (with OpenCV, for JPG/PNG input) and
# runs the Node/Express API that wraps it.
#
# Deploy this to a platform that runs arbitrary Docker containers:
# Render, Railway, Fly.io, a VPS, etc. Vercel cannot run this
# image directly — Vercel only hosts the frontend.

FROM node:20-bookworm AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    libopencv-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY package.json package-lock.json* ./
RUN npm install --omit=dev

COPY ezw_compression.cpp ./
RUN g++ -O2 -std=c++17 -DUSE_OPENCV ezw_compression.cpp -o ezw \
    $(pkg-config --cflags --libs opencv4)

COPY server.js ./

# ---- runtime image ----
FROM node:20-bookworm-slim

# Installing the full -dev package here (not just the runtime .so
# files) is a bit heavier, but the exact runtime library package
# names (e.g. libopencv-core406) change between Debian/OpenCV
# versions, so this is the reliable option.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libopencv-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /app/node_modules ./node_modules
COPY --from=build /app/server.js ./server.js
COPY --from=build /app/ezw ./ezw
RUN chmod +x ./ezw

ENV PORT=8080
ENV EZW_BINARY_PATH=/app/ezw

EXPOSE 8080

CMD ["node", "server.js"]
