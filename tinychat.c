/*
 * tinychat.c - [Starting code for] a web-based chat server.
 *  Christian Roy, Dec 2016
 * Based on:
 *  tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *      GET method to serve static and dynamic content.
 *   Tiny Web server
 *   Dave O'Hallaron
 *   Carnegie Mellon University
 */
#include "csapp.h"
#include "dictionary.h"
#include "more_string.h"

/* Handles HTTP requests/responses transaction */
void doit(int fd);

dictionary_t *read_requesthdrs(rio_t *rp);

void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *d);

void parse_query(const char *uri, dictionary_t *d);

void serve_form(int fd, const char *pre_content);

void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

void serve_conversation(int fd, const char* topic, const char* name, const char *pre_content);
void conversation_request_form(int fd, const char *pre_content);
void *go_doit(void *connfdp);

static void print_stringdictionary(dictionary_t *d);

dictionary_t *topics;
dictionary_t *topics2;

int main(int argc, char **argv) 
{
  // Set up file descriptors
  int listenfd, connfd;

  char hostname[MAXLINE], port[MAXLINE];

  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Begin listening on a given socket
  listenfd = Open_listenfd(argv[1]);

  /* Don't kill the server if there's an error, because
     we want to survive errors due to a client. But we
     do want to report errors. */
  exit_on_error(0);

  topics = make_dictionary(COMPARE_CASE_INSENS, free);
  topics2 = make_dictionary(COMPARE_CASE_INSENS, free);

  /* Also, don't stop on broken connections: */
  Signal(SIGPIPE, SIG_IGN);

  // Continually accept clients
  while (1) {    
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd >= 0) {
      Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                  port, MAXLINE, 0);
      // Print this after a browser has connected
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      int *connfdp;
      pthread_t th;
      connfdp = malloc(sizeof(int));
      *connfdp = connfd;
      Pthread_create(&th, NULL, go_doit, connfdp);
      Pthread_detach(th);
    }
  }
}

void *go_doit(void *connfdp){
  int connfd = *(int *)connfdp;
  free(connfdp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) 
{
  char buf[MAXLINE], *method, *uri, *version;
  rio_t rio;
  dictionary_t *headers, *query;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return;
  printf("%s", buf);
  
  if (!parse_request_line(buf, &method, &uri, &version)) {
    clienterror(fd, method, "400", "Bad Request",
                "TinyChat did not recognize the request");
  } else {
    if (strcasecmp(version, "HTTP/1.0")
        && strcasecmp(version, "HTTP/1.1")) {
      clienterror(fd, version, "501", "Not Implemented",
                  "TinyChat does not implement that version");
    } else if (strcasecmp(method, "GET")
               && strcasecmp(method, "POST")) {
      clienterror(fd, method, "501", "Not Implemented",
                  "TinyChat does not implement that method");
    } else {
      headers = read_requesthdrs(&rio);

      /* Parse all query arguments into a dictionary */
      query = make_dictionary(COMPARE_CASE_SENS, free);
      parse_uriquery(uri, query);
      if (!strcasecmp(method, "POST"))
        read_postquery(&rio, headers, query);

      /* For debugging, print the dictionary */
      print_stringdictionary(query);

      // Post requests are handled when the user types in the url into the address bar
      if (strcasecmp(method, "POST")){

        if (!strcasecmp(uri, "/")){
          /* The start code sends back a text-field form: */
          serve_form(fd, "Welcome to TinyChat");
        }
        else if (starts_with("/conversation", uri))
        {
          char *topic = dictionary_get(query, "topic");
          char *topic2 = entity_encode(topic);
          char *conversation = dictionary_get(topics2, topic2);
          if (conversation != NULL){
            conversation_request_form(fd, conversation);
          }
          else {
            conversation_request_form(fd, strdup(""));
          }
        }
        else if (starts_with("/say", uri)){
          char *user = dictionary_get(query, "user");
          char *topic = dictionary_get(query, "topic");
          char *content = dictionary_get(query, "content");
          char *appended;
          char *appended2;
          
          if (user != NULL && topic != NULL){
            char* topic2 = entity_encode(topic);
            char *conversation = dictionary_get(topics, topic2);
            char *conversation2 = dictionary_get(topics2, topic2);

            // Check to see if this is a new conversation
            if (conversation == NULL){
              conversation = "";
              conversation2 = "";
            }

            // Encode the user and message
            char* user2 = entity_encode(user);
            char* content2 = entity_encode(content);            

            // Append the conversations
            appended = append_strings(conversation, strdup("<p>"), user2, strdup(": "), content2, strdup("</p>"), NULL);
            appended2 = append_strings(conversation2, user2, strdup(": "), content2, strdup("\r\n"), NULL);

            // Save the conversation after appending the message
            dictionary_set(topics, topic2, appended);
            dictionary_set(topics2, topic2, appended2);

            serve_conversation(fd, topic2, user2, dictionary_get(topics, topic2));
          }
        }
      }

      // Handles GET requests, such as when a button is pressed, or enter.
      else if (strcasecmp(method, "GET")){
        char *topic = dictionary_get(query, "topic");

        char *message = dictionary_get(query, "message");
        char *name = dictionary_get(query, "name");
        char *appended;
        char *appended2;

        char* name2 = entity_encode(name);
        char* topic2 = entity_encode(topic);

        char *conversation = dictionary_get(topics, topic2);
        char *conversation2 = dictionary_get(topics2, topic2);

        // If this conversation doesn't exist yet, create it
        if (conversation == NULL){
          conversation = "You created a new topic!";
          conversation2 = "";
        }

        printf("Topic: %s\n", topic);
        printf("Topic2: %s\n", topic2);
        printf("Conversation: %s\n", conversation2);

        // If no message was sent, create an empty string for the message
        if (message == NULL || !strcasecmp(message, strdup(""))){
          message = "";
          appended = append_strings(conversation, message, NULL);
          appended2 = append_strings(conversation2, message, NULL);
        }

        // If there was a message sent, append it to the coversation.
        else{
          char* message2 = entity_encode(message);
          appended = append_strings(conversation, strdup("<p>"), name2, strdup(": "), message2, strdup("</p>"), NULL);
          appended2 = append_strings(conversation2, name2, strdup(": "), message2, strdup("\r\n"), NULL);
        }

        printf("Conversation: %s\n", appended2);

        // Save the conversation after appending the message
        dictionary_set(topics, topic2, appended);
        dictionary_set(topics2, topic2, appended2);

        // Build and send the new page
        serve_conversation(fd, topic2, name2, dictionary_get(topics, topic2));
      }

      /* Clean up */
      free_dictionary(query);
      free_dictionary(headers);
    }

    /* Clean up status line */
    free(method);
    free(uri);
    free(version);
  }
}

/*
 * read_requesthdrs - read HTTP request headers
 */
dictionary_t *read_requesthdrs(rio_t *rp) 
{
  char buf[MAXLINE];
  dictionary_t *d = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, d);
  }
  
  return d;
}

void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *dest)
{
  char *len_str, *type, *buffer;
  int len;
  
  len_str = dictionary_get(headers, "Content-Length");
  len = (len_str ? atoi(len_str) : 0);

  type = dictionary_get(headers, "Content-Type");
  
  buffer = malloc(len+1);
  Rio_readnb(rp, buffer, len);
  buffer[len] = 0;

  if (!strcasecmp(type, "application/x-www-form-urlencoded")) {
    parse_query(buffer, dest);
  }

  free(buffer);
}

static char *ok_header(size_t len, const char *content_type) {
  char *len_str, *header;
  
  header = append_strings("HTTP/1.0 200 OK\r\n",
                          "Server: TinyChat Web Server\r\n",
                          "Connection: close\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n",
                          "Content-type: ", content_type, "\r\n\r\n",
                          NULL);
  free(len_str);

  return header;
}

/*
 * serve_form - sends a form to a client
 */
void serve_form(int fd, const char *pre_content)
{
  size_t len;
  char *body, *header;

  body = append_strings("<html><body>\r\n",
                        "<p>Welcome to TinyChat</p>",
                        "\r\n<form action=\"conversation\" method=\"post\"",
                        " enctype=\"application/x-www-form-urlencoded\"",
                        " accept-charset=\"UTF-8\">\r\n",
                        "Name: <input type=\"text\" name=\"name\"><br>\r\n", 
                        "Topic: <input type=\"text\" name=\"topic\"><br><br>\r\n",                        
                        "<input type=\"submit\" value=\"Join Conversation\">\r\n",
                        "</form></body></html>\r\n",
                        NULL);
  
  len = strlen(body);


  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

/*
 * request_form - sends a form to a client
 */
void conversation_request_form(int fd, const char *pre_content)
{
  size_t len;
  char *body, *header;

  body = append_strings(pre_content, NULL);
  
  len = strlen(body);


  /* Send response headers to client */
  header = ok_header(len, "text/plain; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

/*
 * serve_conversation - sends a form to a client
 */
void serve_conversation(int fd, const char *topic, const char *name, const char *pre_content)
{
  size_t len;
  char *body, *header;

  body = append_strings("<html><style>",
                        "body { background-color: #ffc805}",
                        "h1 { color: #111; font-family: 'Open Sans', sans-serif; font-size: 30px; font-weight: 700; line-height: 32px; margin: 0 0 72px; text-align: center; }",
                        "p { color: #685206; font-family: 'Helvetica Neue', sans-serif; font-size: 14px; font-weight: 700; line-height: 24px; margin: 0 0 24px; text-align: justify; text-justify: inter-word; }",
                        "</style><body>\r\n",
                        "<h1>", topic, "</h1>",
                        "\r\n<form action=\"conversation\" method=\"post\"",
                        " enctype=\"application/x-www-form-urlencoded\"",
                        " accept-charset=\"UTF-8\">\r\n",
                        "<p>", pre_content, "</p><p>",
                        name, ": <input type=\"text\" name=\"message\"></p>\r\n",                      
                        "<input type=\"submit\" value=\"Send\">\r\n",
                        "<input type=\"hidden\" name=\"topic\" value=\"", topic, "\">",
                        "<input type=\"hidden\" name=\"name\" value=\"", name, "\">",
                        "</form></body></html>\r\n",
                        NULL);
  
  len = strlen(body);


  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
  size_t len;
  char *header, *body, *len_str;

  body = append_strings("<html><title>Tiny Error</title>",
                        "<body bgcolor=""ffffff"">\r\n",
                        errnum, " ", shortmsg,
                        "<p>", longmsg, ": ", cause,
                        "<hr><em>The Tiny Web server</em>\r\n",
                        NULL);
  len = strlen(body);

  /* Print the HTTP response */
  header = append_strings("HTTP/1.0 ", errnum, " ", shortmsg,
                          "Content-type: text/html; charset=utf-8\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n\r\n",
                          NULL);
  free(len_str);
  
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, body, len);

  free(header);
  free(body);
}

static void print_stringdictionary(dictionary_t *d)
{
  int i, count;

  count = dictionary_count(d);
  for (i = 0; i < count; i++) {
    printf("%s=%s\n",
           dictionary_key(d, i),
           (const char *)dictionary_value(d, i));
  }
  printf("\n");
}
