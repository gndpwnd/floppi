ssh devel@192.168.0.7


rsync -avz --exclude='.git' --exclude='*.zip' --exclude='*.pdf' --exclude='*.png' /home/devel/floppi/ devel@192.168.0.7:~/Desktop/floppi/


