# Rest in C Major

> Let your postgres database live in constant fear of (un)REST.

## Build

```bash
gcc -o rest-in-c-major rest-in-c-major.c -lulfius -ljansson -lpq
```

## Run

```bash
./rest-in-c-major "postgres..." 5000
```
