ARG PLATFORM=${BUILDPLATFORM:-linux/amd64}
ARG IMAGE=debian:bookworm

FROM --platform=${PLATFORM} ${IMAGE}

RUN if test -f "/etc/debian_version"; then \
        apt-get update \
 &&     apt-get install -y "git" "ssh" "gcc" "g++" "clang-16" "clang++-16" \
                           "cmake" "make" "lcov" \
 &&     update-alternatives --install "/usr/bin/clang" "clang" "$(which clang-16)" 100 \
 &&     update-alternatives --install "/usr/bin/clang++" "clang++" "$(which clang++-16)" 100; \
    elif test -f "/etc/alpine-release"; then \
        apk add --no-cache "git" "openssh" "gcc" "g++" "clang" "cmake" \
                           "make" "lcov"; \
    else \
        echo "Unsupported distribution" && exit 1; \
    fi

COPY . "/libcorrect"

WORKDIR "/libcorrect"

ENV CTEST_OUTPUT_ON_FAILURE=1

CMD [ "/libcorrect/container_ci.sh" ]
