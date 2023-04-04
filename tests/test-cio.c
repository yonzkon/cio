#include <sched.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cio.h"

#define TCP_ADDR "127.0.0.1:1224"
#define TOKEN_LISTENER 1
#define TOKEN_STREAM 2

/**
 * client
 */

static int client_finished = 0;

static void *client_thread(void *args)
{
    (void)args;

    sleep(1);

    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return NULL;

    uint32_t host;
    uint16_t port;
    char *tmp = strdup(TCP_ADDR);
    char *colon = strchr(tmp, ':');
    *colon = 0;
    host = inet_addr(tmp);
    port = htons(atoi(colon + 1));
    free(tmp);

    int rc = 0;
    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family = PF_INET;
    sockaddr.sin_addr.s_addr = host;
    sockaddr.sin_port = port;

    rc = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (rc == -1) {
        perror("connect");
        close(fd);
        assert_true(rc != -1);
        return NULL;
    }

    struct cio *ctx = cio_new();
    cio_register(ctx, fd, TOKEN_STREAM, CIOF_READABLE | CIOF_WRITABLE, NULL);
    for (;;) {
        if (client_finished)
            break;
        assert_true(cio_poll(ctx, 100 * 1000) == 0);
        for (;;) {
            struct cio_event *ev = cio_iter(ctx);
            if (!ev) break;
            printf("fetch a event on client: %c,%d\n",
                   cioe_get_token(ev),
                   cioe_get_code(ev));
            switch (cioe_get_token(ev)) {
                case TOKEN_STREAM: {
                    int fd = cioe_get_fd(ev);
                    int code = cioe_get_code(ev);
                    if (code == CIOE_WRITABLE) {
                        char *payload = "from client";
                        int nr = send(fd, payload, strlen(payload), 0);
                        printf("[client:send]: nr:%d, buf:%s\n", nr, payload);
                        cio_unregister(ctx, fd);
                        cio_register(ctx, fd, TOKEN_STREAM, CIOF_READABLE, NULL);
                    } else if (code == CIOE_READABLE) {
                        char buf[256] = {0};
                        int nr = recv(fd, buf, sizeof(buf), 0);
                        printf("[client:recv]: nr:%d, buf:%s\n", nr, buf);
                        client_finished = 1;
                    }
                    break;
                }
            }
        }
    }
    cio_drop(ctx);

    close(fd);
    sleep(1);
    return NULL;
}

/**
 * server
 */

static int server_finished = 0;

static void *server_thread(void *args)
{
    (void)args;

    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return NULL;

    uint32_t host;
    uint16_t port;
    char *tmp = strdup(TCP_ADDR);
    char *colon = strchr(tmp, ':');
    *colon = 0;
    host = inet_addr(tmp);
    port = htons(atoi(colon + 1));
    free(tmp);

    int rc = 0;
    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family = PF_INET;
    sockaddr.sin_addr.s_addr = host;
    sockaddr.sin_port = port;

    rc = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (rc == -1) {
        perror("bind");
        close(fd);
        assert_true(rc != -1);
        return NULL;
    }

    rc = listen(fd, 100);
    if (rc == -1) {
        perror("listen");
        close(fd);
        assert_true(rc != -1);
        return NULL;
    }

    struct cio *ctx = cio_new();
    cio_register(ctx, fd, TOKEN_LISTENER, CIOF_READABLE, NULL);
    for (;;) {
        if (server_finished)
            break;
        assert_true(cio_poll(ctx, 100 * 1000) == 0);
        for (;;) {
            struct cio_event *ev = cio_iter(ctx);
            if (!ev) break;
            printf("fetch a event on server: %c,%d\n",
                   cioe_get_token(ev),
                   cioe_get_code(ev));
            switch (cioe_get_token(ev)) {
                case TOKEN_LISTENER: {
                    int fd = cioe_get_fd(ev);
                    int code = cioe_get_code(ev);
                    if (code == CIOE_READABLE) {
                        int new_fd = accept(fd, NULL, NULL);
                        cio_register(ctx, new_fd, TOKEN_STREAM, CIOF_READABLE, NULL);
                    }
                    break;
                }
                case TOKEN_STREAM: {
                    int fd = cioe_get_fd(ev);
                    int code = cioe_get_code(ev);
                    if (code == CIOE_READABLE) {
                        char buf[256] = {0};
                        int nr = recv(fd, buf, sizeof(buf), 0);
                        printf("[server:recv]: nr:%d, buf:%s\n", nr, buf);
                        char *payload = "from server";
                        send(fd, payload, strlen(payload), 0);
                        printf("[server:send]: nr:%d, buf:%s\n", nr, payload);
                        server_finished = 1;
                    }
                    break;
                }
            }
        }
    }
    cio_drop(ctx);

    close(fd);
    sleep(1);
    return NULL;
}

/**
 * test_cio
 */

static void test_cio(void **status)
{
    (void)status;

    pthread_t server_pid;
    pthread_create(&server_pid, NULL, server_thread, NULL);
    pthread_t client_pid;
    pthread_create(&client_pid, NULL, client_thread, NULL);

    for (;;) {
        if (client_finished && server_finished)
            break;
    }

    pthread_join(client_pid, NULL);
    pthread_join(server_pid, NULL);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cio),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
