 
import os
import sys
import time

from random import choice
from string import ascii_lowercase, digits

def main():
    
    while True:
		
        key = ''.join(choice(ascii_lowercase) for i in range(3))
        message = ''.join(choice(ascii_lowercase) for i in range(10))
        _id = ''.join(choice(digits) for i in range(2))
        
        node = open('/dev/your_cool_crypto_device', 'w')
        node.write('encrypt {key} {message} {id}'.format(key=key, message=message, id=_id))
        print("Sent encrypt command to device...encrypt {key} {message} {id}\n".format(key=key, message=message, id=_id))
        node.close()
        
        time.sleep(1)
        
        node = open('/dev/your_cool_crypto_device', 'w')
        node.write('decrypt {key} {id}'.format(key=key, id=_id))
        print("Sent decrypt command to device...decrypt {key} {id}\n".format(key=key, id=_id))
        node.close()
        
        time.sleep(5)
        
        node = open('/dev/your_cool_crypto_device', 'r')
        answ = node.read().strip()
        node.close()
        print("testing with working timer:\n\
            source message: {msg},\n\
            decrypted message: {answ}\n\
            correctness: {c}\n".format(msg=message, answ=answ, c=message==answ))
        
        time.sleep(6)
        node = open('/dev/your_cool_crypto_device', 'r')
        answ = node.read().strip()
        node.close()
        print("testing with expired timer\n")
        if not answ:
            print("message unavailable. all good\n")
        else:
            print("error, message should be removed but its not: %s\n", answ)

    
if __name__=="__main__":
    main()
