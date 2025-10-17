
mkdir -p logs
iwasm_versions/iwasm_printout -f 'func' WASM_files/aes.wasm 131 | sed -E 's/, Perf/\nPerf/' > logs/aes.log
iwasm_versions/iwasm_printout -f 'func' WASM_files/aes_O1.wasm 131 | sed -E 's/, Perf/\nPerf/' > logs/aes_O1.log
iwasm_versions/iwasm_printout -f 'func' WASM_files/aes_O2.wasm 131 | sed -E 's/, Perf/\nPerf/' > logs/aes_O2.log

#Allready at search depth 2, O0 generates 200M commands. Search depth 1 > ~8M.
iwasm_versions/iwasm_printout -f 'one_move' WASM_files/chess.wasm 1 | sed -E 's/, Perf/\nPerf/' > logs/chess.log
#Search depth 3, O1/O2 generates 100M commands. Search depth 2 > ~50M commands.
iwasm_versions/iwasm_printout -f 'one_move' WASM_files/chess_O1.wasm 3 | sed -E 's/, Perf/\nPerf/' > logs/chess_O1.log
iwasm_versions/iwasm_printout -f 'one_move' WASM_files/chess_O2.wasm 3 | sed -E 's/, Perf/\nPerf/' > logs/chess_O2.log

iwasm_versions/iwasm_printout -f 'func' WASM_files/ref_c.wasm | sed -E 's/, Perf/\nPerf/' > logs/ref_c.log
iwasm_versions/iwasm_printout -f 'func' WASM_files/ref_calls.wasm | sed -E 's/, Perf/\nPerf/' > logs/ref_calls.log

iwasm_versions/iwasm_printout -f 'main' WASM_files/ref_fingerprints.wasm 1 | sed -E 's/, Perf/\nPerf/' > logs/fingerprints_full.log
iwasm_versions/iwasm_printout -f 'main' WASM_files/ref_fingerprints.wasm 0xdeadbeef | sed -E 's/, Perf/\nPerf/' > logs/fingerprints_early_ret.log

iwasm_versions/iwasm_printout -f '_start' WASM_files/primes.wasm | sed -E 's/, Perf/\nPerf/' > logs/primes.log
iwasm_versions/iwasm_printout -f 'main' WASM_files/montgom.wasm | sed -E 's/, Perf/\nPerf/' > logs/montgom.log
