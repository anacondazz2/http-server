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

RUN groupadd -r dummyuser && useradd -r -g dummyuser dummyuser

WORKDIR /server

COPY --from=builder /server/build/http-server .
COPY --from=builder /server/public ./public

RUN chown -R dummyuser:dummyuser /server

USER dummyuser

EXPOSE 8081

CMD ["./http-server"]
