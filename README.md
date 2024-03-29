# Producer - Consumer Flow Control 

Problem
---------
- Producer - (single) consumer program with dynamic message rate adjustment. The consumer shall consume messages at a given rate, that is, with a given delay simulating the consumed message usage. An actor (task or process) separate from producer and consumer shall periodically check the message queue length and if the length is below a given threshold, it will increase the production rate. Otherwise (i.e. the message length is above the given threshold), it will decrease the production rate.

Software
---------
- C Programing
- Java Programming
- Makefile
- Gradle
- Prometheus
- Grafana

Usage
--------- 
- `/producer-consumer-orchestrator/observability$ prometheus --config.file=./prometheus/prometheus.yml`
- `/producer-consumer-orchestrator/observability$ ./gradlew app:run`
- `/producer-consumer-orchestrator/prodcons$ ./run.sh`

System Design
---------
<img src = https://github.com/thecuongthehieu/producer-consumer-orchestrator/blob/master/documents/images/System_Design.png>  
