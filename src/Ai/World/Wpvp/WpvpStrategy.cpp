#include "WpvpStrategy.h"

void WpvpExcursionStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Below "attack enemy player" (55.0f) so a flagged target always wins.
    triggers.push_back(
        new TriggerNode(
            "wpvp goad",
            {
                NextAction("wpvp goad", 50.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "wpvp shadowmeld",
            {
                NextAction("shadowmeld", 20.0f)
            }
        )
    );
}
