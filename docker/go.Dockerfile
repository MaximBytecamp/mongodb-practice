FROM golang:1.25

# go.sum дописывается при первом запуске; модули кэшируются в томе go-modules
ENV GOFLAGS=-mod=mod

COPY docker/run.sh /usr/local/bin/run
RUN sed -i 's/\r$//' /usr/local/bin/run && chmod +x /usr/local/bin/run
