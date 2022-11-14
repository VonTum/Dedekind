#include "commands.h"

#include "../dedelib/flatPCoeffProcessing.h"

#include "../dedelib/pcoeffValidator.h"

template<unsigned int Variables>
void processDedekindNumberWithBasicValidator(void (*processorFunc)(PCoeffProcessingContext& context)) {
	processDedekindNumber(Variables, processorFunc, basicValidatorPThread<Variables>);
}

template<unsigned int Variables>
void processDedekindNumberWithContinuousValidator(void (*processorFunc)(PCoeffProcessingContext& context)) {
	processDedekindNumber(Variables, processorFunc, continuousValidatorPThread<Variables>);
}

CommandSet processingCommands {"Massively parallel Processing Commands", {
	{"processDedekindNumber1_ST", []() {processDedekindNumber(1, cpuProcessor_SingleThread<1>); }},
	{"processDedekindNumber2_ST", []() {processDedekindNumber(2, cpuProcessor_SingleThread<2>); }},
	{"processDedekindNumber3_ST", []() {processDedekindNumber(3, cpuProcessor_SingleThread<3>); }},
	{"processDedekindNumber4_ST", []() {processDedekindNumber(4, cpuProcessor_SingleThread<4>); }},
	{"processDedekindNumber5_ST", []() {processDedekindNumber(5, cpuProcessor_SingleThread<5>); }},
	{"processDedekindNumber6_ST", []() {processDedekindNumber(6, cpuProcessor_SingleThread<6>); }},
	{"processDedekindNumber7_ST", []() {processDedekindNumber(7, cpuProcessor_SingleThread<7>); }},

	{"processDedekindNumber1_CMT", []() {processDedekindNumber(1, cpuProcessor_CoarseMultiThread<1>); }},
	{"processDedekindNumber2_CMT", []() {processDedekindNumber(2, cpuProcessor_CoarseMultiThread<2>); }},
	{"processDedekindNumber3_CMT", []() {processDedekindNumber(3, cpuProcessor_CoarseMultiThread<3>); }},
	{"processDedekindNumber4_CMT", []() {processDedekindNumber(4, cpuProcessor_CoarseMultiThread<4>); }},
	{"processDedekindNumber5_CMT", []() {processDedekindNumber(5, cpuProcessor_CoarseMultiThread<5>); }},
	{"processDedekindNumber6_CMT", []() {processDedekindNumber(6, cpuProcessor_CoarseMultiThread<6>); }},
	{"processDedekindNumber7_CMT", []() {processDedekindNumber(7, cpuProcessor_CoarseMultiThread<7>); }},

	{"processDedekindNumber1_FMT", []() {processDedekindNumber(1, cpuProcessor_FineMultiThread<1>); }},
	{"processDedekindNumber2_FMT", []() {processDedekindNumber(2, cpuProcessor_FineMultiThread<2>); }},
	{"processDedekindNumber3_FMT", []() {processDedekindNumber(3, cpuProcessor_FineMultiThread<3>); }},
	{"processDedekindNumber4_FMT", []() {processDedekindNumber(4, cpuProcessor_FineMultiThread<4>); }},
	{"processDedekindNumber5_FMT", []() {processDedekindNumber(5, cpuProcessor_FineMultiThread<5>); }},
	{"processDedekindNumber6_FMT", []() {processDedekindNumber(6, cpuProcessor_FineMultiThread<6>); }},
	{"processDedekindNumber7_FMT", []() {processDedekindNumber(7, cpuProcessor_FineMultiThread<7>); }},

	{"processDedekindNumber1_SMT", []() {processDedekindNumber(1, cpuProcessor_SuperMultiThread<1>); }},
	{"processDedekindNumber2_SMT", []() {processDedekindNumber(2, cpuProcessor_SuperMultiThread<2>); }},
	{"processDedekindNumber3_SMT", []() {processDedekindNumber(3, cpuProcessor_SuperMultiThread<3>); }},
	{"processDedekindNumber4_SMT", []() {processDedekindNumber(4, cpuProcessor_SuperMultiThread<4>); }},
	{"processDedekindNumber5_SMT", []() {processDedekindNumber(5, cpuProcessor_SuperMultiThread<5>); }},
	{"processDedekindNumber6_SMT", []() {processDedekindNumber(6, cpuProcessor_SuperMultiThread<6>); }},
	{"processDedekindNumber7_SMT", []() {processDedekindNumber(7, cpuProcessor_SuperMultiThread<7>); }},

	{"processDedekindNumber1_FMT_BasicValidated", []() {processDedekindNumberWithBasicValidator<1>(cpuProcessor_FineMultiThread<1>); }},
	{"processDedekindNumber2_FMT_BasicValidated", []() {processDedekindNumberWithBasicValidator<2>(cpuProcessor_FineMultiThread<2>); }},
	{"processDedekindNumber3_FMT_BasicValidated", []() {processDedekindNumberWithBasicValidator<3>(cpuProcessor_FineMultiThread<3>); }},
	{"processDedekindNumber4_FMT_BasicValidated", []() {processDedekindNumberWithBasicValidator<4>(cpuProcessor_FineMultiThread<4>); }},
	{"processDedekindNumber5_FMT_BasicValidated", []() {processDedekindNumberWithBasicValidator<5>(cpuProcessor_FineMultiThread<5>); }},
	{"processDedekindNumber6_FMT_BasicValidated", []() {processDedekindNumberWithBasicValidator<6>(cpuProcessor_FineMultiThread<6>); }},
	{"processDedekindNumber7_FMT_BasicValidated", []() {processDedekindNumberWithBasicValidator<7>(cpuProcessor_FineMultiThread<7>); }},

	{"processDedekindNumber1_FMT_ContinuousValidated", []() {processDedekindNumberWithContinuousValidator<1>(cpuProcessor_FineMultiThread<1>); }},
	{"processDedekindNumber2_FMT_ContinuousValidated", []() {processDedekindNumberWithContinuousValidator<2>(cpuProcessor_FineMultiThread<2>); }},
	{"processDedekindNumber3_FMT_ContinuousValidated", []() {processDedekindNumberWithContinuousValidator<3>(cpuProcessor_FineMultiThread<3>); }},
	{"processDedekindNumber4_FMT_ContinuousValidated", []() {processDedekindNumberWithContinuousValidator<4>(cpuProcessor_FineMultiThread<4>); }},
	{"processDedekindNumber5_FMT_ContinuousValidated", []() {processDedekindNumberWithContinuousValidator<5>(cpuProcessor_FineMultiThread<5>); }},
	{"processDedekindNumber6_FMT_ContinuousValidated", []() {processDedekindNumberWithContinuousValidator<6>(cpuProcessor_FineMultiThread<6>); }},
	{"processDedekindNumber7_FMT_ContinuousValidated", []() {processDedekindNumberWithContinuousValidator<7>(cpuProcessor_FineMultiThread<7>); }},
}, {}};
