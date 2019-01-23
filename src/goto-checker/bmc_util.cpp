/*******************************************************************\

Module: Bounded Model Checking Utilities

Author: Daniel Kroening, Peter Schrammel

\*******************************************************************/

/// \file
/// Bounded Model Checking Utilities

#include "bmc_util.h"

#include <fstream>
#include <iostream>

#include <goto-programs/graphml_witness.h>
#include <goto-programs/json_goto_trace.h>
#include <goto-programs/xml_goto_trace.h>

#include <goto-symex/build_goto_trace.h>
#include <goto-symex/memory_model_pso.h>
#include <goto-symex/slice.h>
#include <goto-symex/symex_target_equation.h>

#include <linking/static_lifetime_init.h>

#include <solvers/prop/prop_conv.h>

#include <util/make_unique.h>
#include <util/ui_message.h>

#include "symex_bmc.h"

void build_error_trace(
  goto_tracet &goto_trace,
  const namespacet &ns,
  const symex_target_equationt &symex_target_equation,
  const prop_convt &prop_conv,
  ui_message_handlert &ui_message_handler)
{
  messaget msg(ui_message_handler);
  msg.status() << "Building error trace" << messaget::eom;

  build_goto_trace(symex_target_equation, prop_conv, ns, goto_trace);
}

void output_error_trace(
  const goto_tracet &goto_trace,
  const namespacet &ns,
  const trace_optionst &trace_options,
  ui_message_handlert &ui_message_handler)
{
  messaget msg(ui_message_handler);
  switch(ui_message_handler.get_ui())
  {
  case ui_message_handlert::uit::PLAIN:
    msg.result() << "Counterexample:" << messaget::eom;
    show_goto_trace(msg.result(), ns, goto_trace, trace_options);
    msg.result() << messaget::eom;
    break;

  case ui_message_handlert::uit::XML_UI:
  {
    xmlt xml;
    convert(ns, goto_trace, xml);
    msg.status() << xml;
  }
  break;

  case ui_message_handlert::uit::JSON_UI:
  {
    json_stream_objectt &json_result =
      ui_message_handler.get_json_stream().push_back_stream_object();
    const goto_trace_stept &step = goto_trace.steps.back();
    json_result["property"] =
      json_stringt(step.pc->source_location.get_property_id());
    json_result["description"] =
      json_stringt(step.pc->source_location.get_comment());
    json_result["status"] = json_stringt("failed");
    json_stream_arrayt &json_trace =
      json_result.push_back_stream_array("trace");
    convert<json_stream_arrayt>(ns, goto_trace, json_trace, trace_options);
  }
  break;
  }
}

/// outputs an error witness in graphml format
void output_graphml(
  const goto_tracet &goto_trace,
  const namespacet &ns,
  const optionst &options)
{
  const std::string graphml = options.get_option("graphml-witness");
  if(graphml.empty())
    return;

  graphml_witnesst graphml_witness(ns);
  graphml_witness(goto_trace);

  if(graphml == "-")
    write_graphml(graphml_witness.graph(), std::cout);
  else
  {
    std::ofstream out(graphml);
    write_graphml(graphml_witness.graph(), out);
  }
}

/// outputs a proof in graphml format
void output_graphml(
  const symex_target_equationt &symex_target_equation,
  const namespacet &ns,
  const optionst &options)
{
  const std::string graphml = options.get_option("graphml-witness");
  if(graphml.empty())
    return;

  graphml_witnesst graphml_witness(ns);
  graphml_witness(symex_target_equation);

  if(graphml == "-")
    write_graphml(graphml_witness.graph(), std::cout);
  else
  {
    std::ofstream out(graphml);
    write_graphml(graphml_witness.graph(), out);
  }
}

void convert_symex_target_equation(
  symex_target_equationt &equation,
  prop_convt &prop_conv,
  message_handlert &message_handler)
{
  messaget msg(message_handler);
  msg.status() << "converting SSA" << messaget::eom;

  // convert SSA
  equation.convert(prop_conv);
}

std::unique_ptr<memory_model_baset>
get_memory_model(const optionst &options, const namespacet &ns)
{
  const std::string mm = options.get_option("mm");

  if(mm.empty() || mm == "sc")
    return util_make_unique<memory_model_sct>(ns);
  else if(mm == "tso")
    return util_make_unique<memory_model_tsot>(ns);
  else if(mm == "pso")
    return util_make_unique<memory_model_psot>(ns);
  else
  {
    throw "invalid memory model '" + mm + "': use one of sc, tso, pso";
  }
}

void setup_symex(
  symex_bmct &symex,
  const namespacet &ns,
  const optionst &options,
  ui_message_handlert &ui_message_handler)
{
  messaget msg(ui_message_handler);
  const symbolt *init_symbol;
  if(!ns.lookup(INITIALIZE_FUNCTION, init_symbol))
    symex.language_mode = init_symbol->mode;

  msg.status() << "Starting Bounded Model Checking" << messaget::eom;

  symex.last_source_location.make_nil();

  symex.unwindset.parse_unwind(options.get_option("unwind"));
  symex.unwindset.parse_unwindset(options.get_option("unwindset"));
}

void slice(
  symex_bmct &symex,
  symex_target_equationt &symex_target_equation,
  const namespacet &ns,
  const optionst &options,
  ui_message_handlert &ui_message_handler)
{
  messaget msg(ui_message_handler);

  // any properties to check at all?
  if(symex_target_equation.has_threads())
  {
    // we should build a thread-aware SSA slicer
    msg.statistics() << "no slicing due to threads" << messaget::eom;
  }
  else
  {
    if(options.get_bool_option("slice-formula"))
    {
      ::slice(symex_target_equation);
      msg.statistics() << "slicing removed "
                       << symex_target_equation.count_ignored_SSA_steps()
                       << " assignments" << messaget::eom;
    }
    else
    {
      if(options.get_list_option("cover").empty())
      {
        simple_slice(symex_target_equation);
        msg.statistics() << "simple slicing removed "
                         << symex_target_equation.count_ignored_SSA_steps()
                         << " assignments" << messaget::eom;
      }
    }
  }
  msg.statistics() << "Generated " << symex.get_total_vccs() << " VCC(s), "
                   << symex.get_remaining_vccs()
                   << " remaining after simplification" << messaget::eom;
}

std::vector<irep_idt> update_properties_status_from_symex_target_equation(
  propertiest &properties,
  const symex_target_equationt &equation)
{
  std::vector<irep_idt> updated_properties;

  for(const auto &step : equation.SSA_steps)
  {
    if(!step.is_assert())
      continue;

    irep_idt property_id = step.get_property_id();

    if(property_id.empty())
      continue;

    // Don't set false properties; we wouldn't have traces for them.
    const auto status = step.cond_expr.is_true() ? property_statust::PASS
                                                 : property_statust::UNKNOWN;
    auto emplace_result = properties.emplace(
      property_id, property_infot{step.source.pc, step.comment, status});

    if(emplace_result.second)
    {
      updated_properties.push_back(property_id);
    }
    else
    {
      property_infot &property_info = emplace_result.first->second;
      property_statust old_status = property_info.status;
      property_info.status |= status;

      if(property_info.status != old_status)
        updated_properties.push_back(property_id);
    }
  }

  for(auto &property_pair : properties)
  {
    if(property_pair.second.status == property_statust::NOT_CHECKED)
    {
      // This could be a NOT_CHECKED, NOT_REACHABLE or PASS,
      // but the equation doesn't give us precise information.
      property_pair.second.status = property_statust::PASS;
      updated_properties.push_back(property_pair.first);
    }
  }

  return updated_properties;
}
