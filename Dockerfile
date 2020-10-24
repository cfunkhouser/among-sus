FROM debian:latest AS base

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get install -y gcc make libc6-dev

FROM base AS builder

COPY . /build
WORKDIR /build
RUN make

FROM debian:latest
LABEL maintainer="christian@funkhouse.rs"

COPY --from=builder /build/among-sus /bin/among-sus

EXPOSE 1234
CMD [ "/bin/among-sus" ]