
/* (c)  oblong industries */

// Derived from EX5-16 in the sample code:
//
// http://examples.oreilly.com/9780596002701/NSwO-1.3.tar.gz
//
// for the book:
//
// Network Security with OpenSSL by John Viega, Matt Messier, & Pravir Chandra
// Copyright 2002 O'Reilly Media, Inc.  ISBN 978-0-596-00270-1
//
// http://shop.oreilly.com/category/customer-service/faq-examples.do

#include "ossl-common.h"
#include "libLoam/c/ob-sys.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-file.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string.h>

#define BUF_SIZE 16384

/* A couple of little helper functions to make read() and write()
 * return values look more like SSL return values. */
static int fd_read_get_error (int fd, ssize_t code)
{
  if (code > 0)
    return SSL_ERROR_NONE;
  else if (code == 0)
    return SSL_ERROR_ZERO_RETURN;
  else if (errno == EAGAIN || errno == EWOULDBLOCK)
    return SSL_ERROR_WANT_READ;
  else
    return SSL_ERROR_SYSCALL;
}

static int fd_write_get_error (int fd, ssize_t code)
{
  if (code > 0)
    return SSL_ERROR_NONE;
  else if (code == 0)
    return SSL_ERROR_ZERO_RETURN;
  else if (errno == EAGAIN || errno == EWOULDBLOCK)
    return SSL_ERROR_WANT_WRITE;
  else
    return SSL_ERROR_SYSCALL;
}

static void check_availability (ob_sock_t A, unsigned int *can_read_A,
                                unsigned int *can_write_A, SSL *B,
                                ob_sock_t B_sock, unsigned int *can_read_B,
                                unsigned int *can_write_B,
                                bool nonempty_write_buffers)
{
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  fd_set readfds, writefds;
  FD_ZERO (&readfds);
  FD_ZERO (&writefds);
  FD_SET (A, &readfds);
  FD_SET (B_sock, &readfds);
  FD_SET (A, &writefds);
  FD_SET (B_sock, &writefds);
  int max_sock = A;
  if (B_sock > max_sock)
    max_sock = B_sock;
  int pending = SSL_pending (B);
  if (pending < 0)
    OB_LOG_ERROR_CODE (0x20501000, "pending was %d\n", pending);
  else if (ERR_peek_error ())
    OB_LOG_ERROR_CODE (0x20501001, "got an error\n");
#if !defined(                                                                  \
  WIN32) /* on windows, select is a wrapper, and fd's aren't indices into a fixed size array */
  if (max_sock >= FD_SETSIZE || max_sock < 0)
    OB_LOG_ERROR_CODE (0x20501002, "max_sock %d out of range\n", max_sock);
#endif

  /* First, wait for at least one file descriptor to be ready to
   * read.  (Unless SSL_pending is true, meaning there is data ready to
   * be read which is "inside" OpenSSL's buffers, in which case we don't
   * wait.  Or nonempty_write_buffers is true, which means
   * OREILLY_data_transfer has buffered some data which needs to be written.) */
  int ret;
  do
    {
      ret = select (max_sock + 1, &readfds, NULL, NULL,
                    (pending || nonempty_write_buffers) ? &tv : NULL);
    }
  while (ret < 0 && errno == EINTR);

  if (ret < 0)
    OB_PERROR_CODE (0x20501009, "select");

  /* Now, wait for at least one file descriptor to be ready to write, too! */
  do
    {
      ret = select (max_sock + 1, NULL, &writefds, NULL, NULL);
    }
  while (ret < 0 && errno == EINTR);

  if (ret < 0)
    OB_PERROR_CODE (0x2050100d, "select");

  *can_read_A = FD_ISSET (A, &readfds);
  *can_write_A = FD_ISSET (A, &writefds);
  *can_read_B = FD_ISSET (B_sock, &readfds);
  *can_write_B = FD_ISSET (B_sock, &writefds);
  if (pending)
    *can_read_B = 1;
}

void OREILLY_data_transfer (ob_sock_t A, SSL *B)
{
  const int B_sock = SSL_get_fd (B);

#ifdef MSG_NOSIGNAL
  const int send_flags = MSG_NOSIGNAL;
#else
  const int send_flags = 0;
#endif

  /* the buffers and the size variables */
  unsigned char A2B[BUF_SIZE];
  unsigned char B2A[BUF_SIZE];
  unsigned int A2B_len = 0;
  unsigned int B2A_len = 0;
  /* flags to mark that we have some data to write */
  unsigned int have_data_A2B = 0;
  unsigned int have_data_B2A = 0;
  /* flags set by check_availability(  ) that poll for I/O status */
  unsigned int can_read_A = 0;
  unsigned int can_read_B = 0;
  unsigned int can_write_A = 0;
  unsigned int can_write_B = 0;
  /* flags to mark all the combinations of why we're blocking */
  unsigned int read_waiton_write_A = 0;
  unsigned int read_waiton_write_B = 0;
  unsigned int read_waiton_read_A = 0;
  unsigned int read_waiton_read_B = 0;
  unsigned int write_waiton_write_A = 0;
  unsigned int write_waiton_write_B = 0;
  unsigned int write_waiton_read_A = 0;
  unsigned int write_waiton_read_B = 0;
  /* variable to hold return value of an I/O operation */
  int code;
  int interpretation;
  int erryes;
  ob_retort tort;
  /* temporary buffer for error messages */
  char errbuf[160];

  /* make the underlying I/O layer behind each SSL object non-blocking */
  tort = ob_make_socket_nonblocking (A);
  if (tort < OB_OK)
    OB_LOG_ERROR_CODE (0x2050100e, "failed to make %d nonblocking:\n%s\n", A,
                       ob_error_string (tort));
  tort = ob_make_socket_nonblocking (B_sock);
  if (tort < OB_OK)
    OB_LOG_ERROR_CODE (0x2050100f, "failed to make %d nonblocking:\n%s\n",
                       B_sock, ob_error_string (tort));

  SSL_set_mode (B, SSL_MODE_ENABLE_PARTIAL_WRITE
                     | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  for (;;)
    {
      /* check I/O availability and set flags */
      check_availability (A, &can_read_A, &can_write_A, B, B_sock, &can_read_B,
                          &can_write_B, have_data_A2B || have_data_B2A);

      /* this "if" statement reads data from A. it will only be entered if
         * the following conditions are all true:
         * 1. we're not in the middle of a write on A
         * 2. there's space left in the A to B buffer
         * 3. either we need to write to complete a previously blocked read
         *    and now A is available to write, or we can read from A
         *    regardless of whether we're blocking for availability to read.
         */
      if (!(write_waiton_read_A || write_waiton_write_A)
          && (A2B_len != BUF_SIZE)
          && (can_read_A || (can_write_A && read_waiton_write_A)))
        {
          /* clear the flags since we'll set them based on the I/O call's
             * return
             */
          read_waiton_read_A = 0;
          read_waiton_write_A = 0;

          /* read into the buffer after the current position */
          code = recv (A, (char *) (A2B + A2B_len), BUF_SIZE - A2B_len, 0);
          switch (fd_read_get_error (A, code))
            {
              case SSL_ERROR_NONE:
                /* no errors occured.  update the new length and make
                     * sure the "have data" flag is set.
                     */
                A2B_len += code;
                have_data_A2B = 1;
                break;
              case SSL_ERROR_ZERO_RETURN:
                /* connection closed */
                goto end;
              case SSL_ERROR_WANT_READ:
                /* we need to retry the read after A is available for
                     * reading
                     */
                read_waiton_read_A = 1;
                break;
              case SSL_ERROR_WANT_WRITE:
                /* we need to retry the read after A is available for
                     * writing
                     */
                read_waiton_write_A = 1;
                break;
              case SSL_ERROR_SYSCALL:
                OB_PERROR_CODE (0x2050100a, "SSL_ERROR_SYSCALL");
              // fall thru
              default:
                /* ERROR */
                OB_LOG_DEBUG_CODE (0x20501002, "error\n");
                goto err;
            }
        }

      /* this "if" statement is roughly the same as the previous "if"
         * statement with A and B switched
         */
      if (!(write_waiton_read_B || write_waiton_write_B)
          && (B2A_len != BUF_SIZE)
          && (can_read_B || (can_write_B && read_waiton_write_B)))
        {
          read_waiton_read_B = 0;
          read_waiton_write_B = 0;

          code = SSL_read (B, B2A + B2A_len, BUF_SIZE - B2A_len);
          switch ((interpretation = SSL_get_error (B, code)))
            {
              case SSL_ERROR_NONE:
                B2A_len += code;
                have_data_B2A = 1;
                break;
              case SSL_ERROR_ZERO_RETURN:
                goto end;
              case SSL_ERROR_WANT_READ:
                read_waiton_read_B = 1;
                break;
              case SSL_ERROR_WANT_WRITE:
                read_waiton_write_B = 1;
                break;
              default:
                erryes = errno;
                OB_LOG_DEBUG_CODE (
                  0x20501003, "code %d, interpretation %s\n", code,
                  ob_ossl_interpretation_as_string (errbuf, sizeof (errbuf),
                                                    interpretation, erryes));
                goto err;
            }
        }

      /* this "if" statement writes data to A. it will only be entered if
         * the following conditions are all true:
         * 1. we're not in the middle of a read on A
         * 2. there's data in the A to B buffer
         * 3. either we need to read to complete a previously blocked write
         *    and now A is available to read, or we can write to A
         *    regardless of whether we're blocking for availability to write
         */
      if (!(read_waiton_write_A || read_waiton_read_A) && have_data_B2A
          && (can_write_A || (can_read_A && write_waiton_read_A)))
        {
          /* clear the flags */
          write_waiton_read_A = 0;
          write_waiton_write_A = 0;

          /* perform the write from the start of the buffer */
          code = send (A, (const char *) B2A, B2A_len, send_flags);
          switch (fd_write_get_error (A, code))
            {
              case SSL_ERROR_NONE:
                /* no error occured. adjust the length of the B to A
                     * buffer to be smaller by the number bytes written.  If
                     * the buffer is empty, set the "have data" flags to 0,
                     * or else, move the data from the middle of the buffer
                     * to the front.
                     */
                B2A_len -= code;
                if (!B2A_len)
                  have_data_B2A = 0;
                else
                  memmove (B2A, B2A + code, B2A_len);
                break;
              case SSL_ERROR_ZERO_RETURN:
                /* connection closed */
                goto end;
              case SSL_ERROR_WANT_READ:
                /* we need to retry the write after A is available for
                     * reading
                     */
                write_waiton_read_A = 1;
                break;
              case SSL_ERROR_WANT_WRITE:
                /* we need to retry the write after A is available for
                     * writing
                     */
                write_waiton_write_A = 1;
                break;
              case SSL_ERROR_SYSCALL:
                OB_PERROR_CODE (0x2050100c, "SSL_ERROR_SYSCALL");
              // fall thru
              default:
                /* ERROR */
                OB_LOG_DEBUG_CODE (0x20501004, "error\n");
                goto err;
            }
        }

      /* this "if" statement is roughly the same as the previous "if"
         * statement with A and B switched
         */
      if (!(read_waiton_write_B || read_waiton_read_B) && have_data_A2B
          && (can_write_B || (can_read_B && write_waiton_read_B)))
        {
          write_waiton_read_B = 0;
          write_waiton_write_B = 0;

          code = SSL_write (B, A2B, A2B_len);
          switch ((interpretation = SSL_get_error (B, code)))
            {
              case SSL_ERROR_NONE:
                A2B_len -= code;
                if (!A2B_len)
                  have_data_A2B = 0;
                else
                  memmove (A2B, A2B + code, A2B_len);
                break;
              case SSL_ERROR_ZERO_RETURN:
                /* connection closed */
                goto end;
              case SSL_ERROR_WANT_READ:
                write_waiton_read_B = 1;
                break;
              case SSL_ERROR_WANT_WRITE:
                write_waiton_write_B = 1;
                break;
              default:
                /* ERROR */
                erryes = errno;
                OB_LOG_DEBUG_CODE (
                  0x20501005,
                  "code %d, "
                  "interpretation %s, "
                  "descriptor %d\n",
                  code,
                  ob_ossl_interpretation_as_string (errbuf, sizeof (errbuf),
                                                    interpretation, erryes),
                  B_sock);
                goto err;
            }
        }
    }

  OB_LOG_DEBUG_CODE (0x20501006, "error\n");
err:
  /* if we errored, print then before exiting */
  {
    char *errstack = ob_ossl_err_as_string ();
    OB_LOG_ERROR_CODE (0x20501008, "Error(s) occurred\n%s", errstack);
    free (errstack);
  }
end:
  /* close down the connections. set them back to blocking to simplify. */
  tort = ob_make_socket_blocking (A);
  if (tort < OB_OK)
    OB_LOG_ERROR_CODE (0x20501010, "failed to make %d blocking:\n%s\n", A,
                       ob_error_string (tort));
  tort = ob_make_socket_blocking (B_sock);
  if (tort < OB_OK)
    OB_LOG_ERROR_CODE (0x20501011, "failed to make %d blocking:\n%s\n", B_sock,
                       ob_error_string (tort));
}
