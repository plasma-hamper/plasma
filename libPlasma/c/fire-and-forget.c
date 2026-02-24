
/* (c)  oblong industries */

/* Program to run another program in the background */

#include <windows.h>
#include <stdio.h>
#include <string.h>

int main (int argc, char **argv)
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  static char cmd[2048];
  int i;

  ZeroMemory (&si, sizeof (si));
  si.cb = sizeof (si);
  ZeroMemory (&pi, sizeof (pi));

  if (argc < 2)
    {
      printf ("Usage: %s [cmd ...]\n", argv[0]);
      return 1;
    }
  cmd[0] = 0;
  for (i = 1; i < argc; i++)
    {
      strcat (cmd, argv[i]);
      strcat (cmd, " ");
    }

  if (!CreateProcess (NULL,  // No module name (use command line)
                      cmd,
                      NULL,   // Process handle not inheritable
                      NULL,   // Thread handle not inheritable
                      FALSE,  // Set handle inheritance to FALSE
                      CREATE_BREAKAWAY_FROM_JOB | CREATE_NEW_PROCESS_GROUP
                        | DETACHED_PROCESS,
                      NULL,  // Use parent's environment block
                      NULL,  // Use parent's starting directory
                      &si,   // Pointer to STARTUPINFO structure
                      &pi)   // Pointer to PROCESS_INFORMATION structure
  )
    {
      printf ("CreateProcess failed (%d).\n", GetLastError ());
      return 1;
    }

  // [Don't] wait until child process exits.
  //WaitForSingleObject( pi.hProcess, INFINITE );

  // Close process and thread handles.
  CloseHandle (pi.hProcess);
  CloseHandle (pi.hThread);

  return 0;
}
