#include "commands.h"

#include "../dedelib/flatPCoeffProcessing.h"

CommandSet processingCommands {"Massively parallel Processing Commands", {
	{"processDedekindNumber1_ST", []() {processDedekindNumber<1, 32>(cpuProcessor_SingleThread<1>); }},
	{"processDedekindNumber2_ST", []() {processDedekindNumber<2, 32>(cpuProcessor_SingleThread<2>); }},
	{"processDedekindNumber3_ST", []() {processDedekindNumber<3, 32>(cpuProcessor_SingleThread<3>); }},
	{"processDedekindNumber4_ST", []() {processDedekindNumber<4, 32>(cpuProcessor_SingleThread<4>); }},
	{"processDedekindNumber5_ST", []() {processDedekindNumber<5, 32>(cpuProcessor_SingleThread<5>); }},
	{"processDedekindNumber6_ST", []() {processDedekindNumber<6, 64>(cpuProcessor_SingleThread<6>, 500, 100); }},
	{"processDedekindNumber7_ST", []() {processDedekindNumber<7, 32>(cpuProcessor_SingleThread<7>); }},

	{"processDedekindNumber1_CMT", []() {processDedekindNumber<1, 32>(cpuProcessor_CoarseMultiThread<1>); }},
	{"processDedekindNumber2_CMT", []() {processDedekindNumber<2, 32>(cpuProcessor_CoarseMultiThread<2>); }},
	{"processDedekindNumber3_CMT", []() {processDedekindNumber<3, 32>(cpuProcessor_CoarseMultiThread<3>); }},
	{"processDedekindNumber4_CMT", []() {processDedekindNumber<4, 32>(cpuProcessor_CoarseMultiThread<4>); }},
	{"processDedekindNumber5_CMT", []() {processDedekindNumber<5, 32>(cpuProcessor_CoarseMultiThread<5>); }},
	{"processDedekindNumber6_CMT", []() {processDedekindNumber<6, 64>(cpuProcessor_CoarseMultiThread<6>, 500, 100); }},
	{"processDedekindNumber7_CMT", []() {processDedekindNumber<7, 32>(cpuProcessor_CoarseMultiThread<7>); }}
}, {}};
