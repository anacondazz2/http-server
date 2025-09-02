# Stage 1
FROM debian:stable-slim AS builder

RUN apt-get update && apt-get install -y \
  build-essential \
  cmake \
&& rm -rf /var/lib/apt/lists/*

WORKDIR /server

COPY . .

RUN cmake -B ./build -S . && cmake --build ./build

# Stage 2
FROM debian:stable-slim

WORKDIR /server

COPY --from=builder /server/build/http-server .
COPY --from=builder /server/public ./public

EXPOSE 8081

CMD ["./http-server"]
