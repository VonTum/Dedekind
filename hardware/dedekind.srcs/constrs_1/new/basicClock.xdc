create_clock -period 10.000 -name busClk -waveform {0.000 5.000} [get_ports busClk]
create_clock -period 10.000 -name coreClk -waveform {0.000 5.000} [get_ports coreClk]
