FROM ruby:3.3

# bson собирает нативное расширение, поэтому образ полный, а не slim
RUN gem install mongo -v 2.21.0 --no-document

COPY docker/run.sh /usr/local/bin/run
RUN sed -i 's/\r$//' /usr/local/bin/run && chmod +x /usr/local/bin/run
