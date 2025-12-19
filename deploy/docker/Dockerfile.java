FROM openjdk:11-jre-slim

WORKDIR /app

COPY build.gradle gradlew gradlew.bat /app/
COPY gradle /app/gradle/
COPY src /app/src/

RUN ./gradlew build --no-daemon

EXPOSE 8080

CMD ["java", "-jar", "build/libs/distributed-cache.jar"]