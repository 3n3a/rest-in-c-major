FROM debian:latest as builder

RUN apt-get update && \
    apt-get install -y build-essential libulfius-dev uwsc postgresql-server-dev-all libjansson-dev libmicrohttpd-dev wget

COPY Makefile /src/Makefile
COPY rest-in-c-major.c /src/rest-in-c-major.c
WORKDIR /src

RUN make

###################################################

FROM debian:latest

RUN apt-get update && \
    apt-get install -y libulfius-dev libpq-dev libmicrohttpd-dev libjansson-dev

COPY --from=builder /src/bin/rest-in-c-major /usr/local/bin/rest-in-c-major

ENTRYPOINT ["/usr/local/bin/rest-in-c-major"]
CMD ["${DSN:-postgres}", "${PORT:-5000}"]
