# Linux build environment for utaustinvilla3d.
#
# SimSpark / rcssserver3d aren't in nixpkgs, and current SimSpark (0.3.8+) only
# targets Ubuntu 22.04+. So the container route uses Ubuntu + apt (the same
# environment SimSpark's own CI builds in) rather than Nix. devenv.nix remains
# the native dev shell for machines that already have simspark installed.
#
# Build with scripts/build-in-docker.sh (needs colima or Docker Desktop).
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake pkg-config git ca-certificates \
      ruby-dev \
      libboost-all-dev \
      libode-dev \
      libsdl2-dev \
      libfreetype-dev \
      libdevil-dev \
      libgl1-mesa-dev libglu1-mesa-dev freeglut3-dev libglew-dev \
      libx11-dev zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

# --- SimSpark + rcssserver3d, pinned ----------------------------------------
ARG SIMSPARK_REV=18326f649c0c2e5272b2561fb0f22e70441ace61
RUN git clone https://gitlab.com/robocup-sim/SimSpark.git /tmp/simspark \
 && cd /tmp/simspark && git checkout "$SIMSPARK_REV" \
 && cmake -S spark -B spark/build -DCMAKE_INSTALL_PREFIX=/opt/simspark \
 && cmake --build spark/build -j"$(nproc)" --target install \
 && cmake -S rcssserver3d -B rcssserver3d/build \
      -DCMAKE_INSTALL_PREFIX=/opt/simspark -DCMAKE_PREFIX_PATH=/opt/simspark \
 && cmake --build rcssserver3d/build -j"$(nproc)" --target install \
 && rm -rf /tmp/simspark

ENV SPARK_DIR=/opt/simspark
ENV LD_LIBRARY_PATH=/opt/simspark/lib:/opt/simspark/lib/simspark:/opt/simspark/lib/rcssserver3d
ENV PATH=/opt/simspark/bin:$PATH

WORKDIR /src
CMD ["bash", "-c", "cmake . && make -j$(nproc)"]
