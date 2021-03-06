#include "log_converter.h"
#include "date.h"

namespace log_converter {
void build_cb_json(std::ofstream &outfile,
                   const joined_event::joined_event &je) {
  namespace rj = rapidjson;

  float cost = -1.f * reinterpret_cast<const joined_event::cb_joined_event *>(
                          je.get_hold_of_typed_data())
                          ->reward;
  float original_cost =
      -1.f * reinterpret_cast<const joined_event::cb_joined_event *>(
                 je.get_hold_of_typed_data())
                 ->original_reward;
  const auto &interaction_data =
      reinterpret_cast<const joined_event::cb_joined_event *>(
          je.get_hold_of_typed_data())
          ->interaction_data;
  const auto &probabilities = interaction_data.probabilities;
  const auto &actions = interaction_data.actions;

  try {
    rj::Document d;
    d.SetObject();
    rj::Document::AllocatorType &allocator = d.GetAllocator();

    d.AddMember("_label_cost", cost, allocator);

    float label_p =
        probabilities.size() > 0
            ? probabilities[0] * je.interaction_metadata.pass_probability
            : 0.f;
    d.AddMember("_label_probability", label_p, allocator);

    d.AddMember("_label_Action", actions[0], allocator);

    d.AddMember("_labelIndex", actions[0] - 1, allocator);

    bool skip_learn = !je.is_joined_event_learnable();
    if (skip_learn) {
      d.AddMember("_skipLearn", skip_learn, allocator);
    }

    rj::Value v;
    rj::Value outcome_arr(rj::kArrayType);
    for (auto &o : je.outcome_events) {
      rj::Value outcome(rj::kObjectType);
      if (!o.action_taken) {
        outcome.AddMember("v", o.value, allocator);
      }

      v.SetString(o.metadata.event_id.c_str(), allocator);
      outcome.AddMember("EventId", v, allocator);

      outcome.AddMember("ActionTaken", o.action_taken, allocator);
      outcome_arr.PushBack(outcome, allocator);
    }
    d.AddMember("o", outcome_arr, allocator);

    std::string ts_str = date::format(
        "%F %T %Z",
        date::floor<std::chrono::milliseconds>(je.joined_event_timestamp));
    v.SetString(ts_str.c_str(), allocator);
    d.AddMember("Timestamp", v, allocator);

    d.AddMember("Version", "1", allocator);

    v.SetString(interaction_data.eventId.c_str(), allocator);
    d.AddMember("EventId", v, allocator);

    rj::Value action_arr(rj::kArrayType);
    for (auto &action_id : actions) {
      action_arr.PushBack(action_id, allocator);
    }
    d.AddMember("a", action_arr, allocator);

    rj::Document context;
    context.Parse(je.context.c_str());
    d.AddMember("c", context, allocator);

    rj::Value p_arr(rj::kArrayType);
    for (auto &p : probabilities) {
      p_arr.PushBack(p, allocator);
    }
    d.AddMember("p", p_arr, allocator);

    rj::Value vwState;
    vwState.SetObject();
    {
      v.SetString(je.model_id.c_str(), allocator);
      vwState.AddMember("m", v, allocator);
    }
    d.AddMember("VWState", vwState, allocator);

    d.AddMember("_original_label_cost", original_cost, allocator);

    rj::StringBuffer sb;
    rj::Writer<rj::StringBuffer> writer(sb);
    d.Accept(writer);

    outfile << sb.GetString() << std::endl;
  } catch (const std::exception &e) {
    VW::io::logger::log_error(
        "convert events: [{}] from binary to json format failed: [{}].",
        interaction_data.eventId, e.what());
  }
}
} // namespace log_converter
