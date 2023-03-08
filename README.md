

## Steps to run docker container

-  Run `docker build -t perfy/devenv .`

-  Run `docker run -td -e DISPLAY=host.docker.internal:0 -v ~/perf-event-open-examples:/perf-event-open-examples/ --cap-add SYS_ADMIN perfy/devenv`