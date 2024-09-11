# Rest in C Major

> Let your postgres database live in constant fear of (un)REST.

## Install Dependencies

Works for Debian / Ubuntu

```bash
make install-deps
```

## Build

```bash
make
```

## Run

```bash
./rest-in-c-major "postgres..." 5000
```

## Docker Build

```bash
sudo docker build -t ricm .
```

## Docker Run

```bash
sudo docker run -d -p 5000:5000 -e PORT=5000 -e DATABASE_URL="<dsn>" ricm 
```
