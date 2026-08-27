# ---- Build stage ----
# Compiles the binary. This image has g++ and make, but none of that
# ends up in the final image -- it's just used to produce log_monitor.
FROM debian:bookworm-slim AS builder

RUN apt-get update && \
    apt-get install -y --no-install-recommends g++ make && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

# Same flags as the normal build, plus -O2 for a release build and
# -static-libgcc/-static-libstdc++ so the runtime image doesn't need
# to carry matching libstdc++/libgcc shared libraries.
RUN make CXXFLAGS="-std=c++17 -Wall -Wextra -pthread -Iinclude -O2 -static-libgcc -static-libstdc++"

# ---- Runtime stage ----
# Only the compiled binary and a plain base image -- no compiler,
# no source, no build artifacts.
FROM debian:bookworm-slim

WORKDIR /app
COPY --from=builder /build/log_monitor .

# The log file ArgusCAS watches lives outside the container (see
# docker-compose.yml or the -v flag below), mounted here.
RUN mkdir -p /var/log/watched

ENTRYPOINT ["./log_monitor"]
CMD ["/var/log/watched/app.log"]