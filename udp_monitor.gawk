#!/usr/bin/awk -f

BEGIN{RS = "\n"; FS = "[\ \:]*"}

NR > 1			{
				tx_queue += $8
				rx_queue += $9
			}


END{
	print tx_queue
	print rx_queue
	}
