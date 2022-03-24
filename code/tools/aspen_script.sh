#(time ./gbench -source 1 -batch-size 12 -update-dir ~/data/kron-21/bin/ -thread-count 19 -f ~/data/kron-21/adj.txt) > k21_u_serial.txt 2>&1
#sleep 5
#(time ./gbench -source 1 -batch-size 12 -update-dir ~/data/friendster/shuffled/ -thread-count 19 -f ~/data/friendster/adj.txt) > fr_u_serial.txt 2>&1
#sleep 5
#(time ./gbench -source 1 -batch-size 12 -update-dir ~/data/subdomain/shuffled/ -thread-count 19 -arg 0 -f ~/data/subdomain/adj.txt) > sb_u_serial.txt 2>&1
#sleep 5
#(time ./gbench -source 1 -batch-size 12 -update-dir ~/data/twitter_mpi/shuffled/ -thread-count 19 -f ~/data/twitter_mpi/adj.txt) > tw_u_serial.txt 2>&1
sleep 5

(time ./gbench -source 1 -batch-size 12 -update-dir ~/data/kron-21/mix_bins/ -thread-count 19 -f ~/data/kron-21/adj.txt) > k21_del_u12.txt 2>&1
sleep 5
(time ./gbench -source 1 -batch-size 12 -update-dir ~/data/twitter_mpi/mix_bins/ -thread-count 19 -f ~/data/twitter_mpi/adj.txt) > tw_del_u12.txt 2>&1
sleep 5
(time ./gbench -source 1 -batch-size 12 -update-dir ~/data/friendster/mix_bins/ -thread-count 19 -f ~/data/friendster/adj.txt) > fr_del_u12.txt 2>&1
sleep 5
(time ./gbench -source 1 -batch-size 12 -update-dir ~/data/subdomain/mix_bins/ -thread-count 19 -arg 0 -f ~/data/subdomain/adj.txt) > sb_del_u12.txt 2>&1

