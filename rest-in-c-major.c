#include <ulfius.h>
#include <jansson.h>
#include <libpq-fe.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define DEFAULT_PORT 8080
#define URL_PREFIX "/api"

// Ulfius Instance
struct _u_instance instance;

// Signal handler function to stop the Ulfius framework gracefully
void handle_sigint(int sig) {
  printf("\nCaught signal %d, stopping server...\n", sig);
  ulfius_stop_framework(&instance);  // Stops the framework
  ulfius_clean_instance(&instance);  // Cleans up the instance
  exit(0);                           // Exits the program
}

// PostgreSQL DSN connection string, initialized as NULL
char *dsn = NULL;

int global_logger(const struct _u_request *request, struct _u_response *response, void *user_data) {
    // Convert client address to string
    char client_ip[INET6_ADDRSTRLEN];
    struct sockaddr *client_addr = (struct sockaddr *)request->client_address;

    if (client_addr->sa_family == AF_INET) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)client_addr;
        if (inet_ntop(AF_INET, &addr_in->sin_addr, client_ip, sizeof(client_ip)) == NULL) {
            strcpy(client_ip, "Unknown IP");
        }
    } else if (client_addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)client_addr;
        if (inet_ntop(AF_INET6, &addr_in6->sin6_addr, client_ip, sizeof(client_ip)) == NULL) {
            strcpy(client_ip, "Unknown IP");
        }
    } else {
        strcpy(client_ip, "Unknown IP");
    }

    // Get current time in the required format
    time_t now = time(NULL);
    struct tm *timeinfo = gmtime(&now);
    char time_buffer[100];
    strftime(time_buffer, sizeof(time_buffer), "[%d/%b/%Y:%H:%M:%S +0000]", timeinfo);

    // Calculate body size from binary_body if binary_body_size is not available
    size_t body_size = 0;
    if (response->binary_body != NULL) {
        body_size = strlen((char *)response->binary_body);
    }

    // Log request in NCSA Common Log Format
    fprintf(stderr, "%s - - %s \"%s %s HTTP/1.1\" %ld %zu\n",
           client_ip,
           time_buffer,
           request->http_verb,
           request->http_url,
           response->status,
           body_size); // Use computed size from binary_body

    // Continue to the actual route handler
    return U_CALLBACK_CONTINUE;
}

// Function to return empty 200 OK response
int callback_root(const struct _u_request *request, struct _u_response *response, void *user_data) {
  ulfius_set_empty_body_response(response, 200);
  return U_CALLBACK_CONTINUE;
}

// Function to check DB health and return 200 OK if the database is up
int callback_health(const struct _u_request *request, struct _u_response *response, void *user_data) {
  PGconn *conn = PQconnectdb(dsn);
  PGresult *res = PQexec(conn, "SELECT 1;");
  
  int r_http_status;
  char r_message[10];

  if (PQresultStatus(res) == PGRES_TUPLES_OK) {
    r_http_status = 200;
    strncpy(r_message, "ok", sizeof(r_message) - 1);
  } else {
    r_http_status = 500;
    strncpy(r_message, "not ok", sizeof(r_message) - 1);
  }

  json_t *json_response = json_pack("{s:s}",
		  "status", r_message);
  ulfius_set_json_body_response(response, r_http_status, json_response);
  json_decref(json_response);
  
  PQclear(res);
  PQfinish(conn);
  return U_CALLBACK_CONTINUE;
}

// Function to return a list of user-created tables and their schema, owner
int callback_tables(const struct _u_request *request, struct _u_response *response, void *user_data) {
  PGconn *conn = PQconnectdb(dsn);
  PGresult *res = PQexec(conn, "SELECT table_schema, table_name, (SELECT relowner::regrole FROM pg_class WHERE relname = table_name) as table_owner FROM information_schema.tables WHERE table_type='BASE TABLE' AND table_schema NOT IN ('pg_catalog', 'information_schema');");
  
  if (PQresultStatus(res) == PGRES_TUPLES_OK) {
    json_t *json_response = json_array();
    
    for (int i = 0; i < PQntuples(res); i++) {
      json_t *table_info = json_pack("{s:s, s:s, s:s}",
                                     "schema", PQgetvalue(res, i, 0),
                                     "table_name", PQgetvalue(res, i, 1),
                                     "owner", PQgetvalue(res, i, 2));
      json_array_append_new(json_response, table_info);
    }
    
    ulfius_set_json_body_response(response, 200, json_response);
    json_decref(json_response);
  } else {
    json_t *json_response = json_pack("{s:s, s:s}",
		    "status", "not ok",
		    "message", "error while executing query");
    ulfius_set_json_body_response(response, 500, json_response);
    json_decref(json_response);
  }
  
  PQclear(res);
  PQfinish(conn);
  return U_CALLBACK_CONTINUE;
}

// Function to return specific table details: schema, owner, and column details
int callback_table_info(const struct _u_request *request, struct _u_response *response, void *user_data) {
  const char *table_name = u_map_get(request->map_url, "name");
  
  if (table_name == NULL) {
    json_t *json_response = json_pack("{s:s, s:s}",
		    "status", "not ok", 
		    "message", "please enter a table name");
    ulfius_set_json_body_response(response, 400, json_response);
    json_decref(json_response);
  
    return U_CALLBACK_CONTINUE;
  }
  
  PGconn *conn = PQconnectdb(dsn);
  char query[512];
  snprintf(query, sizeof(query), "SELECT table_schema, table_name, (SELECT relowner::regrole FROM pg_class WHERE relname = table_name) as table_owner FROM information_schema.tables WHERE table_type='BASE TABLE' AND table_schema NOT IN ('pg_catalog', 'information_schema') AND table_name='%s';", table_name);
  PGresult *res = PQexec(conn, query);
  
  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
    json_t *json_response = json_pack("{s:s, s:s, s:s}",
                                      "schema", PQgetvalue(res, 0, 0),
				      "table_name", PQgetvalue(res, 0, 1),
                                      "owner", PQgetvalue(res, 0, 2));

    // Fetch column information
    snprintf(query, sizeof(query), "SELECT column_name, data_type FROM information_schema.columns WHERE table_name='%s';", table_name);
    PGresult *res_columns = PQexec(conn, query);
    
    json_t *columns = json_array();
    for (int i = 0; i < PQntuples(res_columns); i++) {
      json_t *column_info = json_pack("{s:s, s:s}",
                                      "column_name", PQgetvalue(res_columns, i, 0),
                                      "data_type", PQgetvalue(res_columns, i, 1));
      json_array_append_new(columns, column_info);
    }
    
    json_object_set_new(json_response, "columns", columns);
    ulfius_set_json_body_response(response, 200, json_response);
    
    json_decref(json_response);
    PQclear(res_columns);
  } else {
    json_t *json_response = json_pack("{s:s, s:s}",
		    "status", "not ok",
		    "message", "table not found");
    ulfius_set_json_body_response(response, 404, json_response);
    json_decref(json_response);
  }
  
  PQclear(res);
  PQfinish(conn);
  return U_CALLBACK_CONTINUE;
}

// Function to return records of a specific table
int callback_table_records(const struct _u_request *request, struct _u_response *response, void *user_data) {
  const char *table_name = u_map_get(request->map_url, "name");
  
  if (table_name == NULL) {
    json_t *json_response = json_pack("{s:s, s:s}",
		    "status", "not ok",
		    "message", "please provide a table name");
    ulfius_set_json_body_response(response, 400, json_response);
    json_decref(json_response);
  
    return U_CALLBACK_CONTINUE;
  }

  PGconn *conn = PQconnectdb(dsn);
  char query[512];
  snprintf(query, sizeof(query), "SELECT * FROM %s;", table_name);
  PGresult *res = PQexec(conn, query);
  
  if (PQresultStatus(res) == PGRES_TUPLES_OK) {
    json_t *json_response = json_array();
    
    for (int i = 0; i < PQntuples(res); i++) {
      json_t *record = json_object();
      
      for (int j = 0; j < PQnfields(res); j++) {
        json_object_set_new(record, PQfname(res, j), json_string(PQgetvalue(res, i, j)));
      }
      
      json_array_append_new(json_response, record);
    }
    
    ulfius_set_json_body_response(response, 200, json_response);
    json_decref(json_response);
  } else {
    json_t *json_response = json_pack("{s:s, s:s}",
		    "status", "not ok",
		    "message", "records for table not found");
    ulfius_set_json_body_response(response, 404, json_response);
    json_decref(json_response);
  }
  
  PQclear(res);
  PQfinish(conn);
  return U_CALLBACK_CONTINUE;
}

// Main function to set up and run the web service
int main(int argc, char *argv[]) {
  int port = DEFAULT_PORT;

  // Argument parsing for port and DSN
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <dsn> [port]\n", argv[0]);
    return 1;
  }

  dsn = argv[1];

  if (argc > 2) {
    port = atoi(argv[2]);
  }

  // Initialize Ulfius framework
  if (ulfius_init_instance(&instance, port, NULL, NULL) != U_OK) {
    fprintf(stderr, "Error initializing instance\n");
    return 1;
  }
 
  // Add Logging Route
  ulfius_add_endpoint_by_val(&instance, "*", NULL, "/*", 999, &global_logger, NULL);

  // Define REST API endpoints
  ulfius_add_endpoint_by_val(&instance, "GET", URL_PREFIX, "/", 1, &callback_root, NULL);
  ulfius_add_endpoint_by_val(&instance, "GET", URL_PREFIX, "/health", 1, &callback_health, NULL);
  ulfius_add_endpoint_by_val(&instance, "GET", URL_PREFIX, "/tables", 1, &callback_tables, NULL);
  ulfius_add_endpoint_by_val(&instance, "GET", URL_PREFIX, "/tables/:name", 3, &callback_table_info, NULL);
  ulfius_add_endpoint_by_val(&instance, "GET", URL_PREFIX, "/tables/:name/records", 2, &callback_table_records, NULL);
  
  // Start the framework
  if (ulfius_start_framework(&instance) == U_OK) {
    fprintf(stderr, "API running on port %d\n", port);

    // Set up signal handler for SIGINT (CTRL + C)
    signal(SIGINT, handle_sigint);

    // Wait indefinitely until CTRL + C is pressed
    while (1) {
      usleep(100000);  // Non-blocking wait
    }
  } else {
    fprintf(stderr, "Error starting the framework\n");
  }
  
  ulfius_clean_instance(&instance);
  
  return 0;
}
