@{
    # Scan bounds per standard, matching the emulator's default provisioning
    # (frequencies in kHz, symbol rates in kSym/s -- the same units the scan
    # dialog and the /dvbscan switches take). A rig provisioned onto other
    # frequencies overrides with Invoke-Suite.ps1 -RangesPath.
    DVBT = @{ FreqStart = 474000;   FreqEnd = 858000;   Bandwidth = 8000;  SymbolRate = 0;     TimeoutSec = 600 }
    ATSC = @{ FreqStart = 473000;   FreqEnd = 695000;   Bandwidth = 6000;  SymbolRate = 0;     TimeoutSec = 400 }
    DVBC = @{ FreqStart = 306000;   FreqEnd = 322000;   Bandwidth = 8000;  SymbolRate = 6875;  TimeoutSec = 300 }
    DVBS = @{ FreqStart = 10714000; FreqEnd = 10774000; Bandwidth = 30000; SymbolRate = 27500; TimeoutSec = 300 }
}
