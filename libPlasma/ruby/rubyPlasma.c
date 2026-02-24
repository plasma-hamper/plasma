
/* (c)  oblong industries */

/* This file is not for redistribution.  Even if you obtained a copy
 * of it by downloading the Greenhouse SDK, you may not redistribute
 * this or any part of the Greenhouse SDK without written permission
 * from oblong industries.
 */

#include "libLoam/c/ob-vers.h"
#include "libLoam/c/ob-dirs.h"
#include "libLoam/c/ob-log.h"
#include "libLoam/c/ob-util.h"
#include "libPlasma/c/c-plasma.h"
#include "ruby.h"

#include "rubySlaw-ilk-macros.h"

// 2.0+
#if defined(HAVE_RUBY_THREAD_H)
#include <ruby/thread.h>
#endif

#ifndef OB_RUBYPLASMA_API
#define OB_RUBYPLASMA_API
#endif

/* Ruby bindings for Plasma */

// 1.8 and 1.9 cross-version encoding niceness from http://tinyurl.com/47fn223
#ifdef HAVE_RUBY_ENCODING_H  // defined (or not) by ruby.h
#include <ruby/encoding.h>
#else
#define rb_enc_find_index(enc) -1
#define rb_enc_associate_index(str, enc)
#endif

// 1.8 and 1.9 cross-version rstring niceness
#if !defined(RSTRING_LEN)
#define RSTRING_LEN(x) (RSTRING (x)->len)
#define RSTRING_PTR(x) (RSTRING (x)->ptr)
#endif

#ifndef RARRAY_LEN
#define RARRAY_LEN(a) RARRAY (a)->len
#endif

// Forward declarations of 1.8/1.9 signal handling nonsense.
// See end of this file for implementation and all the gory details.
typedef VALUE (*blockin_func) (void *arg);
typedef void (*unblockin_func) (void *arg);

static VALUE portable_blocking_region (blockin_func blockin, void *blockin_arg,
                                       unblockin_func unblockin,
                                       void *unblockin_arg);

OB_RUBYPLASMA_API void Init_rubyPlasma (void);

static VALUE rcSlaw__c_initialize (VALUE self);
static VALUE rcSlaw__c_slaw_from_string (VALUE self, VALUE ryaml);

static VALUE rcSlaw__c_freeze_atomic (VALUE self, VALUE rval, VALUE rtyp);
static VALUE rcSlaw__c_freeze_vect (VALUE self, VALUE rnums, VALUE rtyp);
static VALUE rcSlaw__c_freeze_array (VALUE self, VALUE rnums, VALUE rtyp);
static VALUE rcSlaw__c_freeze_list (VALUE self, VALUE rslawx);
static VALUE rcSlaw__c_freeze_map (VALUE self, VALUE rhash, VALUE rarray);
static VALUE rcSlaw__c_freeze_cons (VALUE self, VALUE rcar, VALUE rcdr);
static VALUE rcSlaw__c_freeze_protein (VALUE self, VALUE descrips,
                                       VALUE ingests, VALUE rude);

static VALUE rcSlaw__c_spew_stderr (VALUE self);

static VALUE rcSlaw__c_type_tag (VALUE self);

static VALUE rcSlaw__c_slaw_is_nil (VALUE self);
static VALUE rcSlaw__c_slaw_is_array (VALUE self);
static VALUE rcSlaw__c_slaw_is_list (VALUE self);
static VALUE rcSlaw__c_slaw_is_map (VALUE self);
static VALUE rcSlaw__c_slaw_is_cons (VALUE self);
static VALUE rcSlaw__c_slaw_is_boolean (VALUE self);
static VALUE rcSlaw__c_slaw_is_vect (VALUE self);
static VALUE rcSlaw__c_slaw_is_numeric (VALUE self);

static VALUE rcSlaw__c_slaw_equals (VALUE self, VALUE other);
static VALUE rcSlaw__c_slaw_spaceship (VALUE self, VALUE other);
static VALUE rcSlaw__c_slaw_hash (VALUE self);

static VALUE rcSlaw__c_slaw_numeric_array_count (VALUE self);
static VALUE rcSlaw__c_slaw_list_count (VALUE self);

static VALUE rcSlaw__c_slaw_value (VALUE self);
static VALUE rcSlaw__c_slaw_value_as_string (VALUE self);
static VALUE rcSlaw__c_slaw_protein_descrips (VALUE self);
static VALUE rcSlaw__c_slaw_protein_ingests (VALUE self);
static VALUE rcSlaw__c_slaw_protein_rude_data (VALUE self);
static VALUE rcSlaw__c_slaw_cons_car (VALUE self);
static VALUE rcSlaw__c_slaw_cons_cdr (VALUE self);
static VALUE rcSlaw__c_slaw_list_nth (VALUE self, VALUE index);

static VALUE rcSlaw__c_slaw_to_string (VALUE self);

static VALUE rcSlaw__c_merge_maps (int argc, VALUE *argv);

static VALUE rcPool__c_create (VALUE self, VALUE rname, VALUE options);
static VALUE rcPool__c_dispose (VALUE self, VALUE rname);
static VALUE rcPool__c_list_pools (VALUE self, VALUE server);
static VALUE rcPool__c_rename (VALUE self, VALUE name1, VALUE name2);
static VALUE rcPool__c_is_in_use (VALUE self, VALUE name);

static VALUE rcHose__c_initialize (VALUE self);
static VALUE rcHose__c_get_name (VALUE self);
static VALUE rcHose__c_set_name (VALUE self, VALUE name);
static VALUE rcHose__c_deposit (VALUE self, VALUE prot);
static VALUE rcHose__c_fetch (VALUE self, VALUE amount, VALUE start);
static VALUE rcHose__c_await_next (VALUE self, VALUE rtimeout);
static VALUE rcHose__c_nth (VALUE self, VALUE index);
static VALUE rcHose__c_prev (VALUE self);
static VALUE rcHose__c_rewind (VALUE self);
static VALUE rcHose__c_runout (VALUE self);
static VALUE rcHose__c_tolast (VALUE self);
static VALUE rcHose__c_seekby (VALUE self, VALUE offset);
static VALUE rcHose__c_seekto (VALUE self, VALUE index);
static VALUE rcHose__c_oldest_index (VALUE self);
static VALUE rcHose__c_newest_index (VALUE self);
static VALUE rcHose__c_advance_oldest (VALUE self, VALUE index);
static VALUE rcHose__c_get_info (VALUE self, VALUE hops);
static VALUE rcHose__c_change_options (VALUE self, VALUE options);
static VALUE rcHose__c_withdraw (VALUE self);

static VALUE rcGangBasicAwaiter__c_initialize_gang (VALUE self);
static VALUE rcGangBasicAwaiter__c_await_next (VALUE self, VALUE rtimeout);
static VALUE rcGangBasicAwaiter__c_add_hose (VALUE self, VALUE rhose);
static VALUE rcGangBasicAwaiter__c_remove_hose (VALUE self, VALUE rhose);

static VALUE rcSlawOutputFile__c_output_open_text (VALUE self, VALUE fname);
static VALUE rcSlawOutputFile__c_output_open_binary (VALUE self, VALUE fname);
static VALUE rcSlawOutputFile__c_output_write (VALUE self, VALUE slaw);

static VALUE rcSlawInputFile__c_input_open (VALUE self, VALUE fname);
static VALUE rcSlawInputFile__c_input_read (VALUE self);

static void rubySlaw__slaw_free (void *slaw);
static void rubySlaw_OutputFile__close (void *handle);
static void rubySlaw_InputFile__close (void *handle);


VALUE rcKernelModule;
VALUE rcPlasmaModule;
VALUE rcSlaw;
VALUE rcProtein;
VALUE rcPool;
VALUE rcHose;
VALUE rcHoseGang;
VALUE rcGangBasicAwaiter;
VALUE rcSlawOutputFile;
VALUE rcSlawInputFile;
VALUE rcException;
VALUE rcStandardError;
VALUE rcSlawTypeTagError;
VALUE rcPoolOperationError;
VALUE rcPoolInUseError;
VALUE rcPoolExistsError;
VALUE rcPoolNotFoundError;
VALUE rcPoolNoProteinError;
VALUE rcPoolWithdrawnError;
VALUE rcPoolUnsupportedError;
VALUE rcPoolFrozenError;
VALUE rcPoolFullError;
VALUE rcSlawIOError;

ID id_nil;
ID id_string;
ID id_boolean;
ID id_protein;
ID id_list;
ID id_map;
ID id_cons;
ID id_tributaries;
ID id__freeze;
ID id__derive_type_tag_from_c_slaw;

VALUE from_c_slaw_constructor_args[2];
VALUE from_c_prot_constructor_args[4];

FOR_ALL_NUMERIC_TYPES (DECLAR_TYPE_ID);

FOR_ALL_NUMERIC_BASICS (KERNEL_CONSTRUCTOR_CDEFINE_1, , , );
FOR_ALL_NUMERIC_BASIC_ARRAYS (KERNEL_CONSTRUCTOR_CDEFINE_1, );
FOR_ALL_NUMERIC_2VECTS (KERNEL_CONSTRUCTOR_CDEFINE_2, 2);
FOR_ALL_NUMERIC_3VECTS (KERNEL_CONSTRUCTOR_CDEFINE_3, 3);
FOR_ALL_NUMERIC_4VECTS (KERNEL_CONSTRUCTOR_CDEFINE_4, 4);

static int encoding_binary;
static int encoding_utf8;


static VALUE retort_to_exception (ob_retort tort)
{
  // Although defining a Ruby exception for every retort is probably
  // overkill (presumably that's why it wasn't done to begin with),
  // we should at least differentiate the most likely-to-happen errors,
  // so they can be distinguished by "rescue".
  switch (tort)
    {
      case POOL_IN_USE:
        return rcPoolInUseError;
      case POOL_EXISTS:
        return rcPoolExistsError;
      case POOL_NO_SUCH_POOL:
        return rcPoolNotFoundError;
      case POOL_NO_SUCH_PROTEIN:
        return rcPoolNoProteinError;
      case POOL_NULL_HOSE:
        return rcPoolWithdrawnError;
      case POOL_UNSUPPORTED_OPERATION:
        return rcPoolUnsupportedError;
      case POOL_FROZEN:
        return rcPoolFrozenError;
      case POOL_FULL:
        return rcPoolFullError;
      default:
        return rcPoolOperationError;
    }
}

static inline pool_hose unwrap_hose (VALUE wrapped)
{
  pool_hose *p = (pool_hose *) DATA_PTR (wrapped);
  return *p;
}

static void deref_and_withdraw (pool_hose *p)
{
  pool_hose h = *p;
  free (p);
  if (h)
    {
      ob_retort tort = pool_withdraw (h);
      if (tort < OB_OK)
        OB_LOG_ERROR_CODE (0x24000003, "implicit withdraw: %s\n",
                           ob_error_string (tort));
    }
}

static bool is_hose_in_a_gang (pool_hose ph)
{
  // The API doesn't have a way to check this directly, but we can
  // check it indirectly by creating a new dummy gang, trying to
  // add the hose, and seeing if it fails.
  pool_gang g = NULL;
  ob_retort pret = pool_new_gang (&g);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  ob_retort tort = pool_join_gang (g, ph);
  pool_disband_gang (g, false);
  return (POOL_ALREADY_GANG_MEMBER == tort);
}

OB_RUBYPLASMA_API void Init_rubyPlasma (void)
{
  OB_CHECK_ABI ();

  rcPlasmaModule = rb_define_module ("Plasma");

  rcHose = rb_define_class_under (rcPlasmaModule, "Hose", rb_cObject);
  rcPool = rb_define_class_under (rcPlasmaModule, "Pool", rb_cObject);
  rcSlaw = rb_define_class_under (rcPlasmaModule, "Slaw", rb_cObject);
  rcProtein = rb_define_class_under (rcPlasmaModule, "Protein", rcSlaw);
  rcHoseGang = rb_define_class_under (rcPlasmaModule, "HoseGang", rcSlaw);
  rcGangBasicAwaiter =
    rb_define_class_under (rcPlasmaModule, "GangBasicAwaiter", rcSlaw);
  rcSlawOutputFile =
    rb_define_class_under (rcPlasmaModule, "SlawOutputFile", rb_cObject);
  rcSlawInputFile =
    rb_define_class_under (rcPlasmaModule, "SlawInputFile", rb_cObject);



  rb_define_method (rcSlaw, "_c_initialize",
                    RUBY_METHOD_FUNC (rcSlaw__c_initialize), 0);

  rb_define_module_function (rcSlaw, "_c_slaw_from_string",
                             rcSlaw__c_slaw_from_string, 1);

  rb_define_method (rcSlaw, "_c_freeze_atomic",
                    RUBY_METHOD_FUNC (rcSlaw__c_freeze_atomic), 2);
  rb_define_method (rcSlaw, "_c_freeze_vect",
                    RUBY_METHOD_FUNC (rcSlaw__c_freeze_vect), 2);
  rb_define_method (rcSlaw, "_c_freeze_array",
                    RUBY_METHOD_FUNC (rcSlaw__c_freeze_array), 2);
  rb_define_method (rcSlaw, "_c_freeze_list",
                    RUBY_METHOD_FUNC (rcSlaw__c_freeze_list), 1);
  rb_define_method (rcSlaw, "_c_freeze_map",
                    RUBY_METHOD_FUNC (rcSlaw__c_freeze_map), 2);
  rb_define_method (rcSlaw, "_c_freeze_cons",
                    RUBY_METHOD_FUNC (rcSlaw__c_freeze_cons), 2);
  rb_define_method (rcSlaw, "_c_freeze_protein",
                    RUBY_METHOD_FUNC (rcSlaw__c_freeze_protein), 3);


  rb_define_method (rcSlaw, "_c_spew_stderr",
                    RUBY_METHOD_FUNC (rcSlaw__c_spew_stderr), 0);

  rb_define_method (rcSlaw, "_c_type_tag",
                    RUBY_METHOD_FUNC (rcSlaw__c_type_tag), 0);

  rb_define_method (rcSlaw, "_c_slaw_is_nil",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_is_nil), 0);
  rb_define_method (rcSlaw, "_c_slaw_is_array",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_is_array), 0);
  rb_define_method (rcSlaw, "_c_slaw_is_list",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_is_list), 0);
  rb_define_method (rcSlaw, "_c_slaw_is_map",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_is_map), 0);
  rb_define_method (rcSlaw, "_c_slaw_is_cons",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_is_cons), 0);
  rb_define_method (rcSlaw, "_c_slaw_is_boolean",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_is_boolean), 0);
  rb_define_method (rcSlaw, "_c_slaw_is_vect",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_is_vect), 0);
  rb_define_method (rcSlaw, "_c_slaw_is_numeric",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_is_numeric), 0);


  rb_define_method (rcSlaw, "_c_slaw_equals",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_equals), 1);
  rb_define_method (rcSlaw, "_c_slaw_spaceship",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_spaceship), 1);
  rb_define_method (rcSlaw, "_c_slaw_hash",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_hash), 0);


  rb_define_method (rcSlaw, "_c_slaw_numeric_array_count",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_numeric_array_count), 0);
  rb_define_method (rcSlaw, "_c_slaw_list_count",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_list_count), 0);

  rb_define_method (rcSlaw, "_c_slaw_value",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_value), 0);
  rb_define_method (rcSlaw, "_c_slaw_value_as_string",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_value_as_string), 0);
  rb_define_method (rcSlaw, "_c_slaw_protein_descrips",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_protein_descrips), 0);
  rb_define_method (rcSlaw, "_c_slaw_protein_ingests",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_protein_ingests), 0);
  rb_define_method (rcSlaw, "_c_slaw_protein_rude_data",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_protein_rude_data), 0);
  rb_define_method (rcSlaw, "_c_slaw_cons_car",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_cons_car), 0);
  rb_define_method (rcSlaw, "_c_slaw_cons_cdr",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_cons_cdr), 0);
  rb_define_method (rcSlaw, "_c_slaw_list_nth",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_list_nth), 1);

  rb_define_method (rcSlaw, "_c_slaw_to_string",
                    RUBY_METHOD_FUNC (rcSlaw__c_slaw_to_string), 0);

  rb_define_module_function (rcSlaw, "_c_merge_maps",
                             RUBY_METHOD_FUNC (rcSlaw__c_merge_maps), -1);


  FOR_ALL_NUMERIC_TYPES (INTERN_TYPE_ID);
  id_nil = rb_intern ("nil");
  id_string = rb_intern ("string");
  id_boolean = rb_intern ("boolean");
  id_protein = rb_intern ("protein");
  id_list = rb_intern ("list");
  id_map = rb_intern ("map");
  id_cons = rb_intern ("cons");
  id__freeze = rb_intern ("_freeze");
  id__derive_type_tag_from_c_slaw = rb_intern ("_derive_type_tag_from_c_slaw");
  id_tributaries = rb_intern ("tributaries");

  from_c_slaw_constructor_args[0] = Qnil;
  from_c_slaw_constructor_args[1] = ID2SYM (id__derive_type_tag_from_c_slaw);
  from_c_prot_constructor_args[0] = Qnil;
  from_c_prot_constructor_args[1] = Qnil;
  from_c_prot_constructor_args[2] = Qnil;
  from_c_prot_constructor_args[3] = ID2SYM (id__derive_type_tag_from_c_slaw);


  rcKernelModule = rb_define_module ("Kernel");
  FOR_ALL_NUMERIC_BASICS (KERNEL_CONSTRUCTOR_RDEFINE, , , 1);
  FOR_ALL_NUMERIC_BASIC_ARRAYS (KERNEL_CONSTRUCTOR_RDEFINE, 1);
  FOR_ALL_NUMERIC_2VECTS (KERNEL_CONSTRUCTOR_RDEFINE, 2);
  FOR_ALL_NUMERIC_3VECTS (KERNEL_CONSTRUCTOR_RDEFINE, 3);
  FOR_ALL_NUMERIC_4VECTS (KERNEL_CONSTRUCTOR_RDEFINE, 4);


  rb_define_module_function (rcPool, "_c_create",
                             RUBY_METHOD_FUNC (rcPool__c_create), 2);
  rb_define_module_function (rcPool, "_c_dispose",
                             RUBY_METHOD_FUNC (rcPool__c_dispose), 1);
  rb_define_module_function (rcPool, "_c_list_pools",
                             RUBY_METHOD_FUNC (rcPool__c_list_pools), 1);
  rb_define_module_function (rcPool, "_c_rename",
                             RUBY_METHOD_FUNC (rcPool__c_rename), 2);
  rb_define_module_function (rcPool, "_c_is_in_use",
                             RUBY_METHOD_FUNC (rcPool__c_is_in_use), 1);

  rb_define_method (rcHose, "_c_initialize",
                    RUBY_METHOD_FUNC (rcHose__c_initialize), 0);
  rb_define_method (rcHose, "_c_get_name",
                    RUBY_METHOD_FUNC (rcHose__c_get_name), 0);
  rb_define_method (rcHose, "_c_set_name",
                    RUBY_METHOD_FUNC (rcHose__c_set_name), 1);
  rb_define_method (rcHose, "deposit", RUBY_METHOD_FUNC (rcHose__c_deposit), 1);
  rb_define_method (rcHose, "_c_fetch", RUBY_METHOD_FUNC (rcHose__c_fetch), 2);
  rb_define_method (rcHose, "_c_await_next",
                    RUBY_METHOD_FUNC (rcHose__c_await_next), 1);
  rb_define_method (rcHose, "nth", RUBY_METHOD_FUNC (rcHose__c_nth), 1);
  rb_define_method (rcHose, "prev", RUBY_METHOD_FUNC (rcHose__c_prev), 0);
  rb_define_method (rcHose, "rewind", RUBY_METHOD_FUNC (rcHose__c_rewind), 0);
  rb_define_method (rcHose, "runout", RUBY_METHOD_FUNC (rcHose__c_runout), 0);
  rb_define_method (rcHose, "tolast", RUBY_METHOD_FUNC (rcHose__c_tolast), 0);
  rb_define_method (rcHose, "seekby", RUBY_METHOD_FUNC (rcHose__c_seekby), 1);
  rb_define_method (rcHose, "seekto", RUBY_METHOD_FUNC (rcHose__c_seekto), 1);
  rb_define_method (rcHose, "oldest_index",
                    RUBY_METHOD_FUNC (rcHose__c_oldest_index), 0);
  rb_define_method (rcHose, "newest_index",
                    RUBY_METHOD_FUNC (rcHose__c_newest_index), 0);
  rb_define_method (rcHose, "_c_advance_oldest",
                    RUBY_METHOD_FUNC (rcHose__c_advance_oldest), 1);
  rb_define_method (rcHose, "_c_get_info",
                    RUBY_METHOD_FUNC (rcHose__c_get_info), 1);
  rb_define_method (rcHose, "_c_change_options",
                    RUBY_METHOD_FUNC (rcHose__c_change_options), 1);
  rb_define_method (rcHose, "withdraw", RUBY_METHOD_FUNC (rcHose__c_withdraw),
                    0);

  rcException = rb_define_class ("Exception", rb_cObject);
  rcStandardError = rb_define_class ("StandardError", rcException);
  rcSlawTypeTagError =
    rb_define_class_under (rcPlasmaModule, "SlawTypeTagError", rcStandardError);
  rcPoolOperationError = rb_define_class_under (
    rcPlasmaModule, "PoolOperationError", rcStandardError);
  rcPoolInUseError = rb_define_class_under (rcPlasmaModule, "PoolInUseError",
                                            rcPoolOperationError);
  rcPoolExistsError = rb_define_class_under (rcPlasmaModule, "PoolExistsError",
                                             rcPoolOperationError);
  rcPoolNotFoundError = rb_define_class_under (
    rcPlasmaModule, "PoolNotFoundError", rcPoolOperationError);
  rcPoolNoProteinError = rb_define_class_under (
    rcPlasmaModule, "PoolNoProteinError", rcPoolOperationError);
  rcPoolWithdrawnError = rb_define_class_under (
    rcPlasmaModule, "PoolWithdrawnError", rcPoolOperationError);
  rcPoolUnsupportedError = rb_define_class_under (
    rcPlasmaModule, "PoolUnsupportedError", rcPoolOperationError);
  rcPoolFrozenError = rb_define_class_under (rcPlasmaModule, "PoolFrozenError",
                                             rcPoolOperationError);
  rcPoolFullError = rb_define_class_under (rcPlasmaModule, "PoolFullError",
                                           rcPoolOperationError);

  rb_define_method (rcGangBasicAwaiter, "_c_initialize_gang",
                    RUBY_METHOD_FUNC (rcGangBasicAwaiter__c_initialize_gang),
                    0);
  rb_define_method (rcGangBasicAwaiter, "_c_await_next",
                    RUBY_METHOD_FUNC (rcGangBasicAwaiter__c_await_next), 1);
  rb_define_method (rcGangBasicAwaiter, "_c_add_hose",
                    RUBY_METHOD_FUNC (rcGangBasicAwaiter__c_add_hose), 1);
  rb_define_method (rcGangBasicAwaiter, "_c_remove_hose",
                    RUBY_METHOD_FUNC (rcGangBasicAwaiter__c_remove_hose), 1);

  rb_define_method (rcSlawOutputFile, "_c_output_open_text",
                    RUBY_METHOD_FUNC (rcSlawOutputFile__c_output_open_text), 1);
  rb_define_method (rcSlawOutputFile, "_c_output_open_binary",
                    RUBY_METHOD_FUNC (rcSlawOutputFile__c_output_open_binary),
                    1);
  rb_define_method (rcSlawOutputFile, "_c_output_write",
                    RUBY_METHOD_FUNC (rcSlawOutputFile__c_output_write), 1);

  rb_define_method (rcSlawInputFile, "_c_input_open",
                    RUBY_METHOD_FUNC (rcSlawInputFile__c_input_open), 1);
  rb_define_method (rcSlawInputFile, "_c_input_read",
                    RUBY_METHOD_FUNC (rcSlawInputFile__c_input_read), 0);

  rcSlawIOError =
    rb_define_class_under (rcPlasmaModule, "SlawIOError", rcStandardError);

  ob_set_prog_name (ob_basename (RSTRING_PTR (rb_gv_get ("0"))));
  encoding_binary = rb_enc_find_index ("BINARY");
  encoding_utf8 = rb_enc_find_index ("UTF-8");
}


static VALUE rcSlaw__c_initialize (VALUE self)
{
  rb_iv_set (self, "@_c_slaw", Qnil);
  return Qnil;
}

// this should be a class method
static VALUE rcSlaw__c_freeze_atomic (VALUE self, VALUE rval, VALUE rtyp)
{
  ID rtypID = SYM2ID (rtyp);
  slaw s = NULL;

  if (rtyp == Qnil || rtypID == id_nil)
    s = slaw_nil ();
  else if (rtypID == id_string)
    {
      Check_Type (rval, T_STRING);
      s = slaw_string_from_substring (RSTRING_PTR (rval), RSTRING_LEN (rval));
    }
  else if (rtypID == id_boolean)
    {
      bool cval = true;
      if (rval == Qfalse)
        cval = false;
      s = slaw_boolean (cval);
    }
  else if (rtypID == id_unt8_array || rtypID == id_int8_array)
    {
      Check_Type (rval, T_STRING);
      const void *data = RSTRING_PTR (rval);
      const int64 len = RSTRING_LEN (rval);
      if (rtypID == id_int8_array)
        s = slaw_int8_array ((const int8 *) data, len);
      else
        s = slaw_unt8_array ((const unt8 *) data, len);
    }
  FOR_ALL_NUMERIC_INTEGER_BASICS (FREEZE_INTEGER_BASIC, , , );
  FOR_ALL_NUMERIC_FLOAT_BASICS (FREEZE_FLOAT_BASIC, , , );

  if (!s)
    rb_raise (rcSlawTypeTagError, "bad type-tag '%s'", rb_id2name (rtypID));

  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, rubySlaw__slaw_free, s);
  return wrapped;
}

// and this one's really a class method. takes a ruby array of numeric
// values
static VALUE rcSlaw__c_freeze_vect (VALUE self, VALUE rnums, VALUE rtyp)
{
  ID rtypID = SYM2ID (rtyp);
  slaw s = NULL;

  if (false)
    ;
  FOR_ALL_NUMERIC_FLOAT_BASICS (FREEZE_FLOAT_VECT, v2, , 2);
  FOR_ALL_NUMERIC_FLOAT_BASICS (FREEZE_FLOAT_VECT, v3, , 3);
  FOR_ALL_NUMERIC_FLOAT_BASICS (FREEZE_FLOAT_VECT, v4, , 4);
  FOR_ALL_NUMERIC_INTEGER_BASICS (FREEZE_INTEGER_VECT, v2, , 2);
  FOR_ALL_NUMERIC_INTEGER_BASICS (FREEZE_INTEGER_VECT, v3, , 3);
  FOR_ALL_NUMERIC_INTEGER_BASICS (FREEZE_INTEGER_VECT, v4, , 4);
  else rb_raise (rcSlawTypeTagError, "bad type-tag '%s'", rb_id2name (rtypID));

  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, rubySlaw__slaw_free, s);
  return wrapped;
}

// class method too. ruby array of numeric values, and a type
static VALUE rcSlaw__c_freeze_array (VALUE self, VALUE rnums, VALUE rtyp)
{
  ID rtypID = SYM2ID (rtyp);
  slaw s = NULL;

  int q, len = RARRAY_LEN (rnums);
  if (false)
    ;
  FOR_ALL_NUMERIC_INTEGER_BASICS (FREEZE_INTEGER_ARRAY, , _array, );
  FOR_ALL_NUMERIC_FLOAT_BASICS (FREEZE_FLOAT_ARRAY, , _array, );
  FOR_ALL_NUMERIC_V234FLOAT_ARRAYS (FREEZE_V234FLOAT_ARRAY);
  FOR_ALL_NUMERIC_V234INTEGER_ARRAYS (FREEZE_V234INTEGER_ARRAY);
  else rb_raise (rcSlawTypeTagError, "bad type-tag '%s'", rb_id2name (rtypID));

  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, rubySlaw__slaw_free, s);
  return wrapped;
}

// this one's really a class method, too. takes a ruby array of frozen
// ruby slawx; produces a c slawlist
static VALUE rcSlaw__c_freeze_list (VALUE self, VALUE rslawx)
{
  int q;
  slabu *sb = slabu_new ();
  int len = RARRAY_LEN (rslawx);
  for (q = 0; q < len; q++)
    {
      VALUE rslaw = rb_ary_entry (rslawx, q);
      VALUE wrapped = rb_iv_get (rslaw, "@_c_slaw");
      if (wrapped == Qnil)
        slabu_list_add_x (sb, slaw_nil ());
      else
        slabu_list_add_z (sb, (bslaw) DATA_PTR (wrapped));
    }
  slaw list = slaw_list_f (sb);
  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, rubySlaw__slaw_free, list);
  return wrapped;
}

// and, yes, really a class method. takes a ruby map of frozen slawx
// and a ruby array giving the order of the keys
static VALUE rcSlaw__c_freeze_map (VALUE self, VALUE rhash, VALUE rarray)
{
  int q;
  slabu *sb = slabu_new ();
  int len = RARRAY_LEN (rarray);
  for (q = 0; q < len; q++)
    {
      VALUE rkey = rb_ary_entry (rarray, q);
      VALUE rvalue = rb_hash_aref (rhash, rkey);
      VALUE k_wrapped = rb_iv_get (rkey, "@_c_slaw");
      VALUE v_wrapped = rb_iv_get (rvalue, "@_c_slaw");
      if ((k_wrapped == Qnil) || (v_wrapped == Qnil))
        continue;
      slaw ks = (slaw) DATA_PTR (k_wrapped);
      slaw kv = (slaw) DATA_PTR (v_wrapped);
      slabu_map_put (sb, ks, kv);
    }
  slaw list = slaw_map_f (sb);
  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, rubySlaw__slaw_free, list);
  return wrapped;
}

static VALUE rcSlaw__c_freeze_cons (VALUE self, VALUE rcar, VALUE rcdr)
{
  VALUE car_wrapped = rb_iv_get (rcar, "@_c_slaw");
  VALUE cdr_wrapped = rb_iv_get (rcdr, "@_c_slaw");
  if ((car_wrapped == Qnil) || (cdr_wrapped == Qnil))
    return Qnil;  // fix: throw exception
  slaw car_s = (slaw) DATA_PTR (car_wrapped);
  slaw cdr_s = (slaw) DATA_PTR (cdr_wrapped);
  slaw cons = slaw_cons (car_s, cdr_s);
  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, rubySlaw__slaw_free, cons);
  return wrapped;
}

// again, a class method. takes a ruby-slaw descrips list and a
// ruby-slaw ingests map.
static VALUE rcSlaw__c_freeze_protein (VALUE self, VALUE descrips,
                                       VALUE ingests, VALUE rude)
{
  VALUE d_wrapped = rb_iv_get (descrips, "@_c_slaw");
  VALUE i_wrapped = rb_iv_get (ingests, "@_c_slaw");
  if (!(d_wrapped && i_wrapped))
    return Qnil;
  slaw d = (slaw) DATA_PTR (d_wrapped);
  slaw i = (slaw) DATA_PTR (i_wrapped);
  Check_Type (rude, T_STRING);
  protein prot =
    protein_from_llr (d, i, RSTRING_PTR (rude), RSTRING_LEN (rude));
  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, protein_free, prot);
  return wrapped;
}

// assumes that both self and other are frozen, and so have @_c_slaw.
// returns Qnil if that's not the case
static VALUE rcSlaw__c_slaw_equals (VALUE self, VALUE other)
{
  VALUE s_wrapped = rb_iv_get (self, "@_c_slaw");
  VALUE o_wrapped = rb_iv_get (other, "@_c_slaw");
  if (!(s_wrapped && o_wrapped))
    return Qnil;
  slaw s = (slaw) DATA_PTR (s_wrapped);
  slaw o = (slaw) DATA_PTR (o_wrapped);
  if (slawx_equal (s, o))
    return Qtrue;
  else
    return Qfalse;
}

// assumes that both self and other are frozen, and so have @_c_slaw.
// returns Qnil if that's not the case
static VALUE rcSlaw__c_slaw_spaceship (VALUE self, VALUE other)
{
  VALUE s_wrapped = rb_iv_get (self, "@_c_slaw");
  VALUE o_wrapped = rb_iv_get (other, "@_c_slaw");
  if (!(s_wrapped && o_wrapped))
    return Qnil;
  slaw s = (slaw) DATA_PTR (s_wrapped);
  slaw o = (slaw) DATA_PTR (o_wrapped);
  int cmp = slaw_semantic_compare (s, o);
  return LL2NUM (cmp);
}

static VALUE rcSlaw__c_slaw_hash (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);
  unt64 hash = slaw_hash (s);
  // Use LONG2FIX so we will always have a Fixnum, because the hash table
  // truncates hash codes to the range of Fixnum anyway.  This means
  // we will lose 1 bit on 64-bit machines, and lose 33 bits on 32-bit
  // machines.
  return LONG2FIX (hash);
}

static VALUE rcSlaw__c_spew_stderr (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);
  slaw_spew_overview_to_stderr (s);
  fprintf (stderr, "\n");
  return Qnil;
}

static VALUE _c_type_tag (bslaw s)
{
  if (!s)
    return Qnil;
  if (slaw_is_nil (s))
    return Qnil;
  else if (slaw_is_protein (s))
    return ID2SYM (id_protein);
  else if (slaw_is_map (s))
    return ID2SYM (id_map);
  else if (slaw_is_list (s))
    return ID2SYM (id_list);
  else if (slaw_is_string (s))
    return ID2SYM (id_string);
  else if (slaw_is_boolean (s))
    return ID2SYM (id_boolean);
  else if (slaw_is_cons (s))
    return ID2SYM (id_cons);
  FOR_ALL_NUMERIC_TYPES (CHECK_TYPE_AND_RETURN_SYM);
  return Qfalse;
}

static VALUE rcSlaw__c_type_tag (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);
  return _c_type_tag (s);
}


static VALUE rcSlaw__c_slaw_is_nil (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qfalse;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (slaw_is_nil (s))
    return Qtrue;
  return Qfalse;
}

static VALUE rcSlaw__c_slaw_is_array (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qfalse;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (slaw_is_numeric_array (s))
    return Qtrue;
  return Qfalse;
}

static VALUE rcSlaw__c_slaw_is_list (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qfalse;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (slaw_is_list (s))
    return Qtrue;
  return Qfalse;
}

static VALUE rcSlaw__c_slaw_is_map (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qfalse;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (slaw_is_map (s))
    return Qtrue;
  return Qfalse;
}

static VALUE rcSlaw__c_slaw_is_cons (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qfalse;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (slaw_is_cons (s))
    return Qtrue;
  return Qfalse;
}

static VALUE rcSlaw__c_slaw_is_boolean (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qfalse;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (slaw_is_boolean (s))
    return Qtrue;
  return Qfalse;
}

static VALUE rcSlaw__c_slaw_is_vect (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qfalse;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (slaw_is_numeric_vector (s))
    return Qtrue;
  return Qfalse;
}

static VALUE rcSlaw__c_slaw_is_numeric (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qfalse;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (slaw_is_numeric (s))
    return Qtrue;
  return Qfalse;
}

static VALUE rcSlaw__c_slaw_list_count (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return LONG2NUM (0);
  slaw s = (slaw) DATA_PTR (wrapped);
  int c = slaw_list_count (s);
  return LL2NUM (c);
}

static VALUE rcSlaw__c_slaw_numeric_array_count (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return LONG2NUM (0);
  slaw s = (slaw) DATA_PTR (wrapped);
  int c = slaw_numeric_array_count (s);
  return LL2NUM (c);
}

static VALUE _new_slaw_from_c_slaw (bslaw cslaw, VALUE container,
                                    bool free_on_gc)
{
  VALUE rslaw;
  if (slaw_is_protein (cslaw))
    rslaw = rb_class_new_instance (4, from_c_prot_constructor_args, rcProtein);
  else
    rslaw = rb_class_new_instance (2, from_c_slaw_constructor_args, rcSlaw);

  VALUE wrapped = Data_Wrap_Struct (
    rb_cData, 0, (free_on_gc ? rubySlaw__slaw_free : NULL), (slaw) cslaw);
  rb_iv_set (rslaw, "@ruby_value", Qnil);
  rb_iv_set (rslaw, "@_c_slaw", wrapped);
  rb_iv_set (rslaw, "@type_tag", _c_type_tag (cslaw));
  // hold ref to containing context to prevent a premature slaw_free
  // being called by ruby destructor
  // fix: should we look upwards to get top container?
  rb_iv_set (rslaw, "@_container", container);
  return rslaw;
}

static VALUE rcSlaw__c_slaw_value (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);

  if (slaw_is_list (s))
    {
      unt64 cnt = slaw_list_count (s);
      VALUE rarray = rb_ary_new2 (cnt);
      bslaw s_entry;
      for (s_entry = slaw_list_emit_first (s); s_entry != NULL;
           s_entry = slaw_list_emit_next (s, s_entry))
        rb_ary_push (rarray, _new_slaw_from_c_slaw (s_entry, self, false));
      return rarray;
    }

  if (slaw_is_map (s))
    {
      VALUE rhash = rb_hash_new ();
      bslaw s_key, s_val;
      bslaw s_cons;
      for (s_cons = slaw_list_emit_first (s); s_cons != NULL;
           s_cons = slaw_list_emit_next (s, s_cons))
        {
          s_key = slaw_cons_emit_car (s_cons);
          s_val = slaw_cons_emit_cdr (s_cons);
          rb_hash_aset (rhash, _new_slaw_from_c_slaw (s_key, self, false),
                        _new_slaw_from_c_slaw (s_val, self, false));
        }
      return rhash;
    }

  if (slaw_is_numeric_vector (s))
    {
      if (slaw_is_numeric_array (s))
        {
          if (false)
            ;
          FOR_ALL_NUMERIC_FLOAT_VECTS (COERCE_TO_FLOAT_VECT_ARRAY, _array);
          FOR_ALL_NUMERIC_INTEGER_VECTS (COERCE_TO_INTEGER_VECT_ARRAY, _array);
        }
      if (false)
        ;
      FOR_ALL_NUMERIC_FLOAT_VECTS (COERCE_TO_FLOAT_VECT, );
      FOR_ALL_NUMERIC_INTEGER_VECTS (COERCE_TO_INTEGER_VECT, );
      OB_LOG_BUG_CODE (0x24000000, "fall through\n");
      return Qnil;
    }

  if (slaw_is_numeric_array (s))
    {
      if (false)
        ;
      FOR_ALL_NUMERIC_INTEGER_BASICS (COERCE_TO_INT64_ARRAY, , _array, );
      FOR_ALL_NUMERIC_FLOAT_BASICS (COERCE_TO_FLOAT64_ARRAY, , _array, );
      OB_LOG_BUG_CODE (0x24000001, "need slaw vector array coercion here\n");
      return Qnil;
    }

  if (slaw_is_numeric (s))
    {
      if (false)
        ;
      FOR_ALL_NUMERIC_INTEGER_BASICS (COERCE_TO_INT64, , , );
      FOR_ALL_NUMERIC_FLOAT_BASICS (COERCE_TO_FLOAT, , , );
      return Qnil;
    }

  if (slaw_is_nil (s))
    return Qnil;

  else if (slaw_is_string (s))
    {
      VALUE str = rb_str_new2 (slaw_string_emit (s));
      rb_enc_associate_index (str, encoding_utf8);
      return str;
    }

  else if (slaw_is_boolean (s))
    {
      if (*slaw_boolean_emit (s))
        return Qtrue;
      else
        return Qfalse;
    }

  return Qnil;
}

static VALUE rcSlaw__c_slaw_value_as_string (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);

  const void *data = NULL;
  int64 len = slaw_numeric_array_count (s);
  if (slaw_is_unt8_array (s))
    data = slaw_unt8_array_emit (s);
  else if (slaw_is_int8_array (s))
    data = slaw_int8_array_emit (s);

  if (len <= 0 || !data)
    {
      len = 0;
      data = "";
    }

  VALUE bin = rb_str_new ((const char *) data, len);
  rb_enc_associate_index (bin, encoding_binary);
  return bin;
}

static VALUE rcSlaw__c_slaw_protein_descrips (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);

  if (slaw_is_protein (s))
    return _new_slaw_from_c_slaw (protein_descrips (s), self, false);
  return Qnil;
}

static VALUE rcSlaw__c_slaw_protein_ingests (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);

  if (slaw_is_protein (s))
    return _new_slaw_from_c_slaw (protein_ingests (s), self, false);
  return Qnil;
}

static VALUE rcSlaw__c_slaw_protein_rude_data (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);

  if (slaw_is_protein (s))
    {
      int64 len;
      const void *rude = protein_rude (s, &len);
      VALUE rudestr = rb_str_new ((const char *) rude, len);
      rb_enc_associate_index (rudestr, encoding_binary);
      return rudestr;
    }
  return Qnil;
}

static VALUE rcSlaw__c_slaw_cons_car (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);

  if (slaw_is_cons (s))
    return _new_slaw_from_c_slaw (slaw_cons_emit_car (s), self, false);
  return Qnil;
}

static VALUE rcSlaw__c_slaw_cons_cdr (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);

  if (slaw_is_cons (s))
    return _new_slaw_from_c_slaw (slaw_cons_emit_cdr (s), self, false);
  return Qnil;
}

static VALUE rcSlaw__c_slaw_list_nth (VALUE self, VALUE idx)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);
  if (!slaw_is_list_or_map (s))
    return Qnil;
  bslaw nth = slaw_list_emit_nth (s, NUM2LL (idx));
  if (!nth)
    return Qnil;
  return _new_slaw_from_c_slaw (nth, self, false);
}

static VALUE rcSlaw__c_slaw_to_string (VALUE self)
{
  VALUE wrapped = rb_iv_get (self, "@_c_slaw");
  if (wrapped == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped);
  slaw out = NULL;
  ob_retort tort = slaw_to_string (s, &out);
  if (tort < OB_OK)
    rb_raise (rcSlawIOError, "%s", ob_error_string (tort));
  return _new_slaw_from_c_slaw (out, Qnil, true);
}


static VALUE rcSlaw__c_slaw_from_string (VALUE self, VALUE ryaml)
{
  slaw out = NULL;
  ob_retort tort = slaw_from_string (StringValueCStr (ryaml), &out);
  if (tort < OB_OK)
    rb_raise (rcSlawIOError, "%s", ob_error_string (tort));
  return _new_slaw_from_c_slaw (out, Qnil, true);
}


static VALUE rcSlaw__c_merge_maps (int argc, VALUE *argv)
{
  if (argc < 0 || argc > 1024)  // fairly arbitrary sanity check
    return Qnil;
  bslaw *maps = (bslaw *) alloca (argc * sizeof (bslaw));
  int i;
  for (i = 0; i < argc; i++)
    {
      VALUE wrapped = rb_iv_get (argv[i], "@_c_slaw");
      if (wrapped == Qnil)
        return Qnil;
      maps[i] = (bslaw) DATA_PTR (wrapped);
    }
  slaw merged = slaw_maps_merge_byarray (maps, argc);
  if (!merged)
    return Qnil;
  return _new_slaw_from_c_slaw (merged, Qnil, true);
}


static VALUE rcPool__c_create (VALUE self, VALUE rname, VALUE options)
{
  VALUE wrapped_options_slaw = rb_iv_get (options, "@_c_slaw");
  if (wrapped_options_slaw == Qnil)
    rb_raise (rcStandardError, "unrecoverable error: options slaw not slawed");
  slaw options_slaw = (slaw) DATA_PTR (wrapped_options_slaw);

  protein options_prot = protein_from (NULL, options_slaw);
  ob_retort pret = pool_create (StringValueCStr (rname), "mmap", options_prot);
  protein_free (options_prot);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "trying to create '%s' - %s",
              StringValueCStr (rname), ob_error_string (pret));
  return Qnil;
}

static VALUE rcPool__c_dispose (VALUE self, VALUE rname)
{
  ob_retort pret = pool_dispose (StringValueCStr (rname));
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "trying to dispose of '%s' - %s",
              StringValueCStr (rname), ob_error_string (pret));
  return Qnil;
}

static VALUE rcPool__c_list_pools (VALUE self, VALUE server)
{
  slaw list = NULL;
  const char *server_name;
  if (server == Qnil)
    server_name = NULL;
  else
    server_name = RSTRING_PTR (server);
  ob_retort tort = pool_list_remote (server_name, &list);
  if (tort < OB_OK)
    rb_raise (retort_to_exception (tort), "trying to list pools for '%s' - %s",
              server_name ? server_name : "local", ob_error_string (tort));
  return _new_slaw_from_c_slaw (list, Qnil, true);
}

static VALUE rcPool__c_rename (VALUE self, VALUE name1, VALUE name2)
{
  ob_retort pret = pool_rename (RSTRING_PTR (name1), RSTRING_PTR (name2));
  if (pret < OB_OK)
    rb_raise (retort_to_exception (pret), "trying to rename '%s' to '%s' - %s",
              RSTRING_PTR (name1), RSTRING_PTR (name2), ob_error_string (pret));
  return Qnil;
}

static VALUE rcPool__c_is_in_use (VALUE self, VALUE name)
{
  ob_retort pret = pool_check_in_use (RSTRING_PTR (name));
  if (pret == POOL_IN_USE)
    return Qtrue;
  else if (pret >= OB_OK)
    return Qfalse;
  else
    rb_raise (retort_to_exception (pret), "trying to check '%s' - %s",
              RSTRING_PTR (name), ob_error_string (pret));

  /* Never reached, but apparently Mac OS X ships with a braindead
   * version of clang, which (unlike gcc, or versions of clang built
   * from the svn trunk) does not honor the "noreturn" attribute on
   * rb_raise.  (bug 7320) */
  return Qnil;
}


static VALUE rcHose__c_initialize (VALUE self)
{
  VALUE pname = rb_iv_get (self, "@pool_name");
  pool_hose ph;
  ob_retort pret = pool_participate (StringValueCStr (pname), &ph, NULL);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "bad participate in '%s' - %s",
              StringValueCStr (pname), ob_error_string (pret));

  pool_hose *php = (pool_hose *) malloc (sizeof (pool_hose));
  *php = ph;
  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, deref_and_withdraw, php);
  rb_iv_set (self, "@_pool_hose", wrapped);
  return Qtrue;
}

static VALUE rcHose__c_get_name (VALUE self)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  const char *name = pool_get_hose_name (ph);
  if (!name)
    return Qnil;
  VALUE str = rb_str_new2 (name);
  rb_enc_associate_index (str, encoding_utf8);
  return str;
}

static VALUE rcHose__c_set_name (VALUE self, VALUE name)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  ob_retort tort = pool_set_hose_name (ph, RSTRING_PTR (name));
  if (tort < OB_OK)
    rb_raise (retort_to_exception (tort), "%s", ob_error_string (tort));
  return Qnil;
}

/**
 * Deposit a protein into this pool.  On return, idx is filled in
 * with the index that was assigned to the protein, provided idx
 * is not nil, in which case it's ignored. The usual error
 * codes will mark a null hose (POOL_NULL_HOSE) or a slaw which is
 * not a protein (POOL_NOT_A_PROTEIN). Other possible error
 * conditions:
 * * an errno encapsulated in a retort for system-level errors
 * * POOL_SEMAPHORES_BADTH if required locks couldn't be acquired
 * * POOL_PROTEIN_BIGGER_THAN_POOL
 * * POOL_CORRUPT
 * and, for remote pools, the usual suspects:
 * * POOL_SOCK_BADTH, POOL_SERVER_UNREACH for connectivity problems.
 * * POOL_PROTOCOL_ERROR
 */
static VALUE rcHose__c_deposit (VALUE self, VALUE prot)
{
  VALUE wrapped_slaw = rb_iv_get (prot, "@_c_slaw");
  if (wrapped_slaw == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped_slaw);
  if (!slaw_is_protein (s))
    return Qnil;

  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  int64 idx;
  ob_retort pret = pool_deposit (ph, s, &idx);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return LL2NUM (idx);
}

/**
 * Read the given amount of proteins (if available) from the pool
 * starting at the given index. If the given index is -1 it will start
 * reading from the current index. This function will return an array
 * with the proteins read or nil if an invalid hose is given. It might
 * be that the number of proteins read is lower than the expected
 * amount.
 */
static VALUE rcHose__c_fetch (VALUE self, VALUE amount, VALUE start)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  pool_fetch_op *ops;
  int64 proteins_to_fetch = NUM2LL (amount);
  ops = (pool_fetch_op *) malloc (sizeof (pool_fetch_op) * proteins_to_fetch);
  if (ops == NULL)
    rb_raise (rb_eNoMemError,
              "unable to allocate memory for %" OB_FMT_64 "d operations",
              proteins_to_fetch);

  ob_retort pret;

  int64 start_idx = NUM2LL (start);
  if (start_idx == -1)
    {
      pret = pool_index (ph, &start_idx);
      if (pret != OB_OK)
        {
          free (ops);
          rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));
        }
    }

  int64 i;
  for (i = 0; i < proteins_to_fetch; i++)
    {
      ops[i].idx = start_idx + i;
      ops[i].want_descrips = true;
      ops[i].want_ingests = true;
      ops[i].rude_offset = -1;
      ops[i].rude_length = -1;
      ops[i].tort = -1;
    }

  int64 idx_old, idx_new;
  pool_fetch (ph, ops, proteins_to_fetch, &idx_old, &idx_new);

  VALUE result;
  result = rb_ary_new2 (proteins_to_fetch);

  for (i = 0; i < proteins_to_fetch; i++)
    {
      if (ops[i].tort == 0)
        {
          VALUE rprotein = _new_slaw_from_c_slaw (ops[i].p, Qnil, true);
          rb_iv_set (rprotein, "@hose", self);
          rb_iv_set (rprotein, "@index", LL2NUM (ops[i].idx));
          rb_iv_set (rprotein, "@timestamp", rb_float_new (ops[i].ts));
          rb_ary_store (result, i, rprotein);
        }
    }

  free (ops);

  return result;
}

typedef struct wclosure
{
  pool_hose ph;
  pool_timestamp timestamp;
  pool_timestamp timeout;
  protein prot;
  int64 idx;
  ob_retort pret;
  pool_gang gang;
} wclosure;

static VALUE wclosure_await_next (void *arg)
{
  wclosure *z = (wclosure *) arg;
  z->pret =
    pool_await_next (z->ph, z->timeout, &z->prot, &z->timestamp, &z->idx);
  return Qnil;
}

static void wclosure_unblock_hose (void *arg)
{
  wclosure *z = (wclosure *) arg;
  OB_DIE_ON_ERROR (pool_hose_wake_up (z->ph));
}

static VALUE rcHose__c_await_next (VALUE self, VALUE rtimeout)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;

  wclosure z;
  OB_CLEAR (z);

  z.ph = unwrap_hose (wrapped_ph);
  z.timeout = NUM2DBL (rtimeout);

  z.pret = pool_hose_enable_wakeup (z.ph);
  if (z.pret < OB_OK)
    rb_raise (retort_to_exception (z.pret), "%s", ob_error_string (z.pret));

  portable_blocking_region (wclosure_await_next, &z, wclosure_unblock_hose, &z);
  if (z.pret == POOL_AWAIT_TIMEDOUT)
    return Qnil;
  if (z.pret != OB_OK)
    rb_raise (retort_to_exception (z.pret), "%s", ob_error_string (z.pret));

  VALUE rprotein = _new_slaw_from_c_slaw (z.prot, Qnil, true);
  rb_iv_set (rprotein, "@hose", self);
  rb_iv_set (rprotein, "@index", LL2NUM (z.idx));
  rb_iv_set (rprotein, "@timestamp", rb_float_new (z.timestamp));
  return rprotein;
}

/**
 * Retrieve the protein with the given index.  If ret_ts is not NULL,
 * the timestamp of the protein is returned in it.  Returns
 * POOL_NO_SUCH_PROTEIN if the index is previous to that of the
 * oldest index or if it is after that of the newest index.
 */
static VALUE rcHose__c_nth (VALUE self, VALUE idx)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  pool_timestamp timestamp;
  protein prot;
  ob_retort pret = pool_nth_protein (ph, NUM2LL (idx), &prot, &timestamp);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  // fix: eliminate the code duplication here with await_next, above
  VALUE rprotein = _new_slaw_from_c_slaw (prot, Qnil, true);
  rb_iv_set (rprotein, "@hose", self);
  rb_iv_set (rprotein, "@index", idx);
  rb_iv_set (rprotein, "@timestamp", rb_float_new (timestamp));
  return rprotein;
}

/**
 * Retrieve the protein just previous to the pool hose's current
 * index.  Move the pool hose's index to this position. If no
 * protein before the current one is available, we return
 * POOL_NO_SUCH_PROTEIN.
 */
static VALUE rcHose__c_prev (VALUE self)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  pool_timestamp timestamp;
  protein prot;
  int64 idx;
  ob_retort pret = pool_prev (ph, &prot, &timestamp, &idx);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  // fix: eliminate the code duplication here with await_next, above
  VALUE rprotein = _new_slaw_from_c_slaw (prot, Qnil, true);
  rb_iv_set (rprotein, "@hose", self);
  rb_iv_set (rprotein, "@index", LL2NUM (idx));
  rb_iv_set (rprotein, "@timestamp", rb_float_new (timestamp));
  return rprotein;
}

/**
 * Set the pool hose's index to the first available protein.
 */
static VALUE rcHose__c_rewind (VALUE self)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  ob_retort pret = pool_rewind (ph);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return Qnil;  // fix: maybe return the current index?
}

/**
 * Set the pool hose's index to that following the last available protein.
 */
static VALUE rcHose__c_runout (VALUE self)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  ob_retort pret = pool_runout (ph);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return Qnil;  // fix: maybe return the current index?
}

/**
 * Set the pool hose's index to the last available protein.
 */
static VALUE rcHose__c_tolast (VALUE self)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  ob_retort pret = pool_tolast (ph);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return Qnil;  // fix: maybe return the current index?
}

/**
 * Move the pool hose's index forward by the given offset.
 */
static VALUE rcHose__c_seekby (VALUE self, VALUE offset)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  ob_retort pret = pool_seekby (ph, NUM2LL (offset));
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return Qnil;  // fix: maybe return the current index?
}

/**
 * Set the pool hose's index to the given value.
 */
static VALUE rcHose__c_seekto (VALUE self, VALUE idx)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  ob_retort pret = pool_seekto (ph, NUM2LL (idx));
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return Qnil;  // fix: maybe return the current index?
}

/**
 * Get the index of the oldest protein in this pool.  Returns
 * POOL_NO_SUCH_PROTEIN if no proteins are in the pool.
 */
static VALUE rcHose__c_oldest_index (VALUE self)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  int64 idx = -1;
  ob_retort pret = pool_oldest_index (ph, &idx);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return LL2NUM (idx);
}

/**
 * Get the index of the newest protein in this pool.  Returns
 * POOL_NO_SUCH_PROTEIN if no proteins are in the pool.
 */
static VALUE rcHose__c_newest_index (VALUE self)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  int64 idx = -1;
  ob_retort pret = pool_newest_index (ph, &idx);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return LL2NUM (idx);
}

static VALUE rcHose__c_advance_oldest (VALUE self, VALUE idx)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  ob_retort tort = pool_advance_oldest (ph, NUM2LL (idx));
  if (tort < OB_OK)
    rb_raise (retort_to_exception (tort), "%s", ob_error_string (tort));
  return Qnil;
}

static VALUE rcHose__c_get_info (VALUE self, VALUE hops)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  protein p = NULL;
  ob_retort pret = pool_get_info (ph, NUM2LL (hops), &p);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return _new_slaw_from_c_slaw (p, Qnil, true);
}

static VALUE rcHose__c_change_options (VALUE self, VALUE options)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);
  VALUE wrapped_options_slaw = rb_iv_get (options, "@_c_slaw");
  if (wrapped_options_slaw == Qnil)
    rb_raise (rcStandardError, "unrecoverable error: options slaw not slawed");
  slaw options_slaw = (slaw) DATA_PTR (wrapped_options_slaw);
  ob_retort pret = pool_change_options (ph, options_slaw);
  if (pret < OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return Qnil;
}

/**
 * Close your connection to the pool and free all resources associated
 * with it.  All (successful) calls to #participate or
 * #pool_participate_creatingly must be followed by a
 * pool_withdraw, eventually. We return POOL_NULL_HOSE if ph is
 * nil. For remote pools, possible error codes are:
 * - POOL_SOCK_BADTH, POOL_SERVER_UNREACH for connectivity problems.
 * - POOL_PROTOCOL_ERROR
 */
static VALUE rcHose__c_withdraw (VALUE self)
{
  VALUE wrapped_ph = rb_iv_get (self, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose *php = (pool_hose *) DATA_PTR (wrapped_ph);
  pool_hose ph = *php;
  if (ph)
    {
      if (is_hose_in_a_gang (ph))
        rb_raise (rcPoolOperationError,
                  "can't withdraw from %s while it is in a gang",
                  pool_name (ph));
      ob_retort pret = pool_withdraw (ph);
      *php = NULL;
      if (pret != OB_OK)
        rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));
    }

  return Qnil;
}



/**
 * Disband the gang and free associated resources.  Does not withdraw,
 * or otherwise affect the hoses that were in the gang.
 * \ingroup PoolGangs
 */
static void r_gang_free (void *gang)
{
  pool_disband_gang ((pool_gang) gang, false);
}

static VALUE rcGangBasicAwaiter__c_initialize_gang (VALUE self)
{
  pool_gang gang;
  ob_retort pret = pool_new_gang (&gang);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  VALUE wrapped = Data_Wrap_Struct (rb_cData, 0, r_gang_free, gang);
  return wrapped;
}

static VALUE wclosure_await_next_multi (void *arg)
{
  wclosure *z = (wclosure *) arg;
  z->pret = pool_await_next_multi (z->gang, z->timeout, &z->ph, &z->prot,
                                   &z->timestamp, &z->idx);
  return Qnil;
}

static void wclosure_unblock_gang (void *arg)
{
  wclosure *z = (wclosure *) arg;
  OB_DIE_ON_ERROR (pool_gang_wake_up (z->gang));
}

static VALUE rcGangBasicAwaiter__c_await_next (VALUE self, VALUE rtimeout)
{
  VALUE wrapped_gang = rb_iv_get (self, "@_hose_gang");
  if (wrapped_gang == Qnil)
    return Qnil;

  wclosure z;
  OB_CLEAR (z);

  z.gang = (pool_gang) DATA_PTR (wrapped_gang);
  z.timeout = NUM2DBL (rtimeout);

  portable_blocking_region (wclosure_await_next_multi, &z,
                            wclosure_unblock_gang, &z);

  // fix: remove POOL_NO_SUCH_PROTEIN when reretorting is finished ?
  if ((z.pret == POOL_AWAIT_TIMEDOUT) || (z.pret == POOL_NO_SUCH_PROTEIN))
    return Qnil;
  if (z.pret != OB_OK)
    rb_raise (retort_to_exception (z.pret), "%s", ob_error_string (z.pret));

  VALUE rprotein = _new_slaw_from_c_slaw (z.prot, Qnil, true);

  // match up our protein's c hose with a ruby hose in the HoseGang's
  // tributaries list
  VALUE r_parent_gang_ref = rb_iv_get (self, "@containing_gang_ref");
  if (r_parent_gang_ref == Qnil)
    {
      OB_LOG_BUG_CODE (0x24000002, "nil container\n");
      return Qnil;
    }
  VALUE r_tributaries = rb_funcall (r_parent_gang_ref, id_tributaries, 0);
  long tribs_count = RARRAY_LEN (r_tributaries);
  int q;
  VALUE r_hose = Qnil;
  for (q = 0; q < tribs_count; q++)
    {
      VALUE r_trib = rb_ary_entry (r_tributaries, q);

      VALUE wrapped_ph = rb_iv_get (r_trib, "@_pool_hose");
      if (wrapped_ph == Qnil)
        continue;
      pool_hose trib_hose = unwrap_hose (wrapped_ph);
      if (trib_hose == z.ph)
        r_hose = r_trib;
    }

  rb_iv_set (rprotein, "@hose", r_hose);
  rb_iv_set (rprotein, "@index", LL2NUM (z.idx));
  rb_iv_set (rprotein, "@timestamp", rb_float_new (z.timestamp));

  return rprotein;
}

static VALUE rcGangBasicAwaiter__c_add_hose (VALUE self, VALUE rhose)
{
  VALUE wrapped_gang = rb_iv_get (self, "@_hose_gang");
  if (wrapped_gang == Qnil)
    return Qnil;
  pool_gang gang = (pool_gang) DATA_PTR (wrapped_gang);

  VALUE wrapped_ph = rb_iv_get (rhose, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  ob_retort pret = pool_join_gang (gang, ph);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return Qnil;
}

static VALUE rcGangBasicAwaiter__c_remove_hose (VALUE self, VALUE rhose)
{
  VALUE wrapped_gang = rb_iv_get (self, "@_hose_gang");
  if (wrapped_gang == Qnil)
    return Qnil;
  pool_gang gang = (pool_gang) DATA_PTR (wrapped_gang);

  VALUE wrapped_ph = rb_iv_get (rhose, "@_pool_hose");
  if (wrapped_ph == Qnil)
    return Qnil;
  pool_hose ph = unwrap_hose (wrapped_ph);

  ob_retort pret = pool_leave_gang (gang, ph);
  if (pret != OB_OK)
    rb_raise (retort_to_exception (pret), "%s", ob_error_string (pret));

  return Qnil;
}


static VALUE rcSlawOutputFile__c_output_open_text (VALUE self, VALUE fname)
{
  slaw_output out_handle;
  ob_retort pret = slaw_output_open_text (StringValueCStr (fname), &out_handle);
  if (pret != OB_OK)
    rb_raise (rcSlawIOError, "could not open '%s' - %s", RSTRING_PTR (fname),
              ob_error_string (pret));

  VALUE wrapped =
    Data_Wrap_Struct (rb_cData, 0, rubySlaw_OutputFile__close, out_handle);
  rb_iv_set (self, "@_c_handle", wrapped);
  return Qnil;
}

static VALUE rcSlawOutputFile__c_output_open_binary (VALUE self, VALUE fname)
{
  slaw_output out_handle;
  ob_retort pret =
    slaw_output_open_binary (StringValueCStr (fname), &out_handle);
  if (pret != OB_OK)
    rb_raise (rcSlawIOError, "could not open '%s' - %s", RSTRING_PTR (fname),
              ob_error_string (pret));

  VALUE wrapped =
    Data_Wrap_Struct (rb_cData, 0, rubySlaw_OutputFile__close, out_handle);
  rb_iv_set (self, "@_c_handle", wrapped);
  return Qnil;
}

static VALUE rcSlawOutputFile__c_output_write (VALUE self, VALUE rslaw)
{
  VALUE wrapped_handle = rb_iv_get (self, "@_c_handle");
  if (wrapped_handle == Qnil)
    rb_raise (rcSlawIOError, "unrecoverable error with output file: "
                             "no internal handle");
  slaw_output handle = (slaw_output) DATA_PTR (wrapped_handle);

  VALUE wrapped_slaw = rb_iv_get (rslaw, "@_c_slaw");
  if (wrapped_slaw == Qnil)
    return Qnil;
  slaw s = (slaw) DATA_PTR (wrapped_slaw);

  ob_retort pret = slaw_output_write (handle, s);
  if (pret != OB_OK)
    rb_raise (rcSlawIOError, "could not write slaw - %s",
              ob_error_string (pret));

  return Qnil;
}


static VALUE rcSlawInputFile__c_input_open (VALUE self, VALUE fname)
{
  slaw_input in_handle;
  ob_retort pret = slaw_input_open (StringValueCStr (fname), &in_handle);
  if (pret != OB_OK)
    rb_raise (rcSlawIOError, "could not open '%s' - %s", RSTRING_PTR (fname),
              ob_error_string (pret));

  VALUE wrapped =
    Data_Wrap_Struct (rb_cData, 0, rubySlaw_InputFile__close, in_handle);
  rb_iv_set (self, "@_c_handle", wrapped);
  return Qnil;
}

static VALUE rcSlawInputFile__c_input_read (VALUE self)
{
  VALUE wrapped_handle = rb_iv_get (self, "@_c_handle");
  if (wrapped_handle == Qnil)
    rb_raise (rcSlawIOError, "unrecoverable error with input file: "
                             "no internal handle");
  slaw_input handle = (slaw_input) DATA_PTR (wrapped_handle);

  slaw s;
  ob_retort pret = slaw_input_read (handle, &s);
  if (pret == SLAW_END_OF_FILE)
    return Qnil;
  else if (pret != OB_OK)
    rb_raise (rcSlawIOError, "slaw input read error - %s",
              ob_error_string (pret));

  return _new_slaw_from_c_slaw (s, Qnil, true);
}


static void rubySlaw__slaw_free (void *fslaw)
{
  // printf ("slaw_free %p\n", slaw);
  slaw_free ((slaw) fslaw);
}

static void rubySlaw_OutputFile__close (void *handle)
{
  // printf ("closing slaw handle %p\n", handle);
  slaw_output_close ((slaw_output) handle);
}

static void rubySlaw_InputFile__close (void *handle)
{
  // printf ("closing slaw handle %p\n", handle);
  slaw_input_close ((slaw_input) handle);
}

#ifndef RUBY_VM /* if defined, then ruby 1.9; else 1.8 */
#include "rubysig.h"
#endif

/* portable_blocking_region() is a wrapper for the Ruby 1.9
 * function rb_thread_blocking_region().  The documentation for
 * that function does not seem to be particularly good or particularly
 * public, but there is some mention of it here:
 *
 * http://blade.nagaokaut.ac.jp/cgi-bin/scat.rb/ruby/ruby-core/10252
 *
 * although note that there it pretends rb_thread_blocking_region()
 * has three arguments, and in fact it has four.  So you can decide
 * how credible that makes that documentation.  It is also documented
 * briefly, and with a "here be dragons" tone, directly in the thread.c
 * source file of the ruby 1.9 distribution.  Since some folks don't
 * like to download the source to read the documentation, I will
 * reproduce the documentation comment here:
 *
 * \*
 *  * rb_thread_blocking_region - permit concurrent/parallel execution.
 *  *
 *  * This function does:
 *  *   (1) release GVL.
 *  *       Other Ruby threads may run in parallel.
 *  *   (2) call func with data1.
 *  *   (3) acquire GVL.
 *  *       Other Ruby threads can not run in parallel any more.
 *  *
 *  *   If another thread interrupts this thread (Thread#kill, signal delivery,
 *  *   VM-shutdown request, and so on), `ubf()' is called (`ubf()' means
 *  *   "un-blocking function").  `ubf()' should interrupt `func()' execution.
 *  *
 *  *   There are built-in ubfs and you can specify these ubfs.
 *  *   However, we can not guarantee our built-in ubfs interrupt
 *  *   your `func()' correctly.  Be careful to use rb_thread_blocking_region().
 *  *
 *  *     * RUBY_UBF_IO: ubf for IO operation
 *  *     * RUBY_UBF_PROCESS: ubf for process operation
 *  *
 *  *   NOTE: You can not execute most of Ruby C API and touch Ruby
 *  *         objects in `func()' and `ubf()', including raising an
 *  *         exception, because current thread doesn't acquire GVL
 *  *         (cause synchronization problem).  If you need to do it,
 *  *         read source code of C APIs and confirm by yourself.
 *  *
 *  *   NOTE: In short, this API is difficult to use safely.  I recommend you
 *  *         use other ways if you have.  We lack experiences to use this API.
 *  *         Please report your problem related on it.
 *  *
 *  *   Safe C API:
 *  *     * rb_thread_interrupted() - check interrupt flag
 *  *     * ruby_xalloc(), ruby_xrealloc(), ruby_xfree() -
 *  *         if they called without GVL, acquire GVL automatically.
 *  *\
 *
 * The situation in Ruby 1.8 is completely different (of course),
 * and even less documented (by which I mean not documented at all),
 * but it appears that one is supposed to use these TRAP_BEG and
 * TRAP_END macros from rubysig.h.  So I have provided an emulation
 * of rb_thread_blocking_region() for 1.8 by using those macros.
 * However, it is particularly disconcerting that the 1.8 version
 * doesn't make use of the unblocking function, which makes me
 * wonder how it unblocks.  I suspect something really evil might
 * be going on, like setjmp/longjmp, in which case the state of the
 * pool hose will almost certainly be corrupted.  But, it at least
 * seems to work in the simple case where all you want to do is
 * hit control-C to exit the program.
 *
 * Why, Matz, why?
 */
static VALUE portable_blocking_region (blockin_func blockin, void *blockin_arg,
                                       unblockin_func unblockin,
                                       void *unblockin_arg)
{
  /* relief for bug 3107 */
  if (getenv ("OB_RUBY_NO_SIGNAL_HANDLING"))
    return blockin (blockin_arg);

#if defined(HAVE_RUBY_THREAD_H)
  /* if defined, then ruby 2.0+ */
  return (VALUE) rb_thread_call_without_gvl ((void *) blockin, blockin_arg,
                                             unblockin, unblockin_arg);
#elif defined(RUBY_VM) /* if defined, then ruby 1.9; else 1.8 */
  return rb_thread_blocking_region (blockin, blockin_arg, unblockin,
                                    unblockin_arg);
#else
  VALUE valyoo = Qnil;
  TRAP_BEG;
  valyoo = blockin (blockin_arg);
  TRAP_END;
  return valyoo;
#endif
}
