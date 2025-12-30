
/* (c)  oblong industries */

#include "libPlasma/c/slaw-io.h"

ob_retort slaw_input_open_text_handler (slaw_read_handler h, slaw_input *f)
{
  return SLAW_NO_YAML;
}

ob_retort slaw_output_open_text_handler (slaw_write_handler h, bslaw options,
                                         slaw_output *f)
{
  return SLAW_NO_YAML;
}

void private_test_yaml_hash (void) {}
