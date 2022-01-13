./run_batch_updates_new -s -m -batch-size 65536 -batch-count 512 -f ~/data/kron-21/k21_empty.txt
./run_batch_updates_file -s -batch-size 65536 -batch-count 512 -f ~/src/graphbolt/apps/k21_empty.txt -update-file ~/data/kron-21/mix/del.txt
