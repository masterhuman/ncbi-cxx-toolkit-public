# $Id$

NCBI_project_tags(test)
NCBI_add_definitions(HAVE_CONFIG_H=1)
NCBI_uses_toolkit_libraries(ct_ftds14)
NCBI_set_test_requires(in-house-resources)

NCBI_add_app(
  test-ct14 t0001 t0002 t0003 t0004 t0005 cs_convert t0007 t0008
  t0009 connect_fail ct_options lang_ct_param array_bind cs_diag
  get_send_data rpc_ct_param rpc_ct_setparam ct_diagclient
  ct_diagserver ct_diagall cs_config cancel blk_in blk_out ct_cursor
  ct_cursors ct_dynamic blk_in2 datafmt rpc_fail row_count all_types
  long_binary will_convert variant timeout
)

NCBI_project_watchers(ucko satskyse)

