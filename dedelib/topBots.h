#pragma once



template<unsigned int Variables>
struct TopBots {
	Monotonic<Variables> top;
	std::vector<Monotonic<Variables>> bots;
};




