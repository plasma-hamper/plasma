static int reap_children (void)
{
  int i;
  int status;
  for (i = 0; i < total_children; i++)
    wait (&status);
}
int main (int argc, char *argv[])
{
  get_args (argc, argv);

  gettimeofday (&start_time, ((void *) 0));
  goal_end_time = start_time;
  goal_end_time.tv_sec += test_secs;

  create_children ();
  reap_children ();
}
