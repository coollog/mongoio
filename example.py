import sys
from subprocess import PIPE, Popen
from threading  import Thread
import pygame
import random
import uuid
import time

try:
    from Queue import Queue, Empty
except ImportError:
    from queue import Queue, Empty  # python 3.x

ON_POSIX = 'posix' in sys.builtin_module_names

def enqueue_output(stdout, queue):
    for line in iter(stdout.readline, b''):
        queue.put(line)
        sys.stdout.write('read: ' + line)
    stdout.close()

def enqueue_input(stdin, queue):
    for line in iter(queue.get, None):
        print 'wrote: ' + line
        stdin.write(line + '\n')
        stdin.flush()
    stdin.close()

if len(sys.argv) < 4:
    print('usage: example.py mongouriRead mongouriWrite tunnelName');
    sys.exit(1)

mongouriRead = sys.argv[1];
mongouriWrite = sys.argv[2];
tunnelName = sys.argv[3];
myName = raw_input('Your name: ');
myID = uuid.uuid4().hex

# The game code

pygame.init()

nameFont = pygame.font.SysFont("monospace", 12)

size = [720, 480]
screen = pygame.display.set_mode(size)
pygame.display.set_caption("MongoIO Example")

lastTime = pygame.time.get_ticks()
frameTicks = 30

class Player():
    def __init__(self, ID, name):
        self.ID = ID
        self.name = name

        self.xSpeed = 0
        self.ySpeed = 0

        self.xCoord = random.randint(100, 600)
        self.yCoord = random.randint(100, 300)

        self.label = nameFont.render(name, 1, (0, 0, 0))
    def coord(self, x, y):
        self.xCoord = x
        self.yCoord = y
    def speed(self, xs, ys):
        self.xSpeed = xs
        self.ySpeed = ys
    def update(self):
        self.xCoord += self.xSpeed
        self.yCoord += self.ySpeed
    def draw(self):
        screen.blit(self.label, (self.xCoord, self.yCoord))
    def sendNewSpeed(self):
        tunnelQueueWrite.put_nowait('state ' + myID + ' ' + str(myPlayer.xCoord) + ' ' + str(myPlayer.yCoord) + ' ' + str(myPlayer.xSpeed) + ' ' + str(myPlayer.ySpeed))

myPlayer = Player(myID, myName)
otherPlayers = []

def gameStep():
    global lastTime, frameTicks, myPlayer, screen

    if pygame.time.get_ticks() - lastTime < frameTicks:
        return False

    # ALL EVENT PROCESSING SHOULD GO BELOW THIS COMMENT
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            return True
        elif event.type == pygame.KEYDOWN:
            if event.key == pygame.K_LEFT:
                myPlayer.xSpeed = -3
                myPlayer.sendNewSpeed()
            elif event.key == pygame.K_RIGHT:
                myPlayer.xSpeed = 3
                myPlayer.sendNewSpeed()
            elif event.key == pygame.K_UP:
                myPlayer.ySpeed = -3
                myPlayer.sendNewSpeed()
            elif event.key == pygame.K_DOWN:
                myPlayer.ySpeed = 3
                myPlayer.sendNewSpeed()
        elif event.type == pygame.KEYUP:
            if event.key == pygame.K_LEFT:
                myPlayer.xSpeed = 0
                myPlayer.sendNewSpeed()
            elif event.key == pygame.K_RIGHT:
                myPlayer.xSpeed = 0
                myPlayer.sendNewSpeed()
            elif event.key == pygame.K_UP:
                myPlayer.ySpeed = 0
                myPlayer.sendNewSpeed()
            elif event.key == pygame.K_DOWN:
                myPlayer.ySpeed = 0
                myPlayer.sendNewSpeed()
    # ALL EVENT PROCESSING SHOULD GO ABOVE THIS COMMENT

    screen.fill((255, 255, 255))
    myPlayer.update()
    myPlayer.draw()
    for p in otherPlayers:
        p.update()
        p.draw()
    pygame.display.flip()

    lastTime = pygame.time.get_ticks()

    return False

# Tunneling system

print 'writing on: ' + mongouriWrite
tunnelWrite = Popen(['node', 'yell', mongouriWrite, tunnelName], stdin=PIPE, bufsize=1, close_fds=ON_POSIX)
tunnelQueueWrite = Queue()
tunnelThreadWrite = Thread(target=enqueue_input, args=(tunnelWrite.stdin, tunnelQueueWrite))
tunnelThreadWrite.daemon = True # thread dies with the program
tunnelThreadWrite.start()

print 'listening on: ' + mongouriRead
tunnelRead = Popen(['node', 'listen', mongouriRead, tunnelName], stdout=PIPE, bufsize=1, close_fds=ON_POSIX)
tunnelQueueRead = Queue()
tunnelThreadRead = Thread(target=enqueue_output, args=(tunnelRead.stdout, tunnelQueueRead))
tunnelThreadRead.daemon = True # thread dies with the program
tunnelThreadRead.start()

# im here
tunnelQueueWrite.put_nowait('newuser ' + myID + ' ' + str(myPlayer.xCoord) + ' ' + str(myPlayer.yCoord) + ' ' + myName)

while True:
    # read line without blocking
    try: line = tunnelQueueRead.get_nowait() # or q.get(timeout=.1)
    except Empty:
        # no output yet
        if gameStep():
            break
    else: # got line
        if ' ' in line:
            line = line[:-1] # remove \n
            mType, mBody = line.split(' ', 1)
            if mType == 'newuser' or mType == 'existinguser':
                pid, px, py, pname = mBody.split(' ', 3)
                exists = False
                if pid != myID:
                    for p in otherPlayers:
                        if p.ID == pid:
                            exists = True
                            break
                    if not exists:
                        newp = Player(pid, pname)
                        newp.coord(int(px), int(py))
                        otherPlayers.append(newp)
                        if mType == 'newuser':
                            tunnelQueueWrite.put_nowait('existinguser ' + myID + ' ' + str(myPlayer.xCoord) + ' ' + str(myPlayer.yCoord) + ' ' + myName)
            elif mType == 'deluser':
                pid = mBody
                for idx, p in enumerate(otherPlayers):
                    if p.ID == pid:
                        del p
                        del otherPlayers[idx]
                        break
            elif mType == 'state':
                pid, px, py, pxs, pys = mBody.split(' ', 4)
                if pid != myID:
                    for p in otherPlayers:
                        if p.ID == pid:
                            p.coord(int(px), int(py))
                            p.speed(int(pxs), int(pys))
                            break

        if gameStep():
            break

tunnelQueueWrite.put_nowait('deluser ' + myID)
time.sleep(1)

tunnelRead.terminate()
tunnelWrite.terminate()
pygame.quit()