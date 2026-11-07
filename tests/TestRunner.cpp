#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    int testCount = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (const auto* result = runner.getResult (i))
        {
            ++testCount;
            failures += result->failures;

            if (result->failures > 0)
            {
                std::cout << "FAILED: " << result->unitTestName << " / " << result->subcategoryName
                          << " (" << result->failures << " failures)\n";

                for (const auto& message : result->messages)
                    std::cout << "  " << message << '\n';
            }
        }
    }

    std::cout << "DarkGrain tests: " << testCount << " run, " << failures << " failure(s)\n";

    return failures == 0 ? 0 : 1;
}
