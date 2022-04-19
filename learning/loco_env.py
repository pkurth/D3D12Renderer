import gym
from gym import error, spaces, utils
from gym.utils import seeding
import numpy as np
import ctypes

class PhysicsDLL() :
    def __init__(self):
        self._physics = ctypes.CDLL('bin/Release_x86_64/Physics-Lib.dll')

        self._physics.updatePhysics.argtypes = (ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float))
        self._physics.resetPhysics.argtypes = (ctypes.POINTER(ctypes.c_float),)
        self._physics.getPhysicsRanges.argtypes = (ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float))

        self.state_size = self._physics.getPhysicsStateSize()
        self.action_size = self._physics.getPhysicsActionSize()
        
        state_min = (ctypes.c_float * self.state_size)(*([0] * self.state_size))
        state_max = (ctypes.c_float * self.state_size)(*([0] * self.state_size))
        action_min = (ctypes.c_float * self.action_size)(*([0] * self.action_size))
        action_max = (ctypes.c_float * self.action_size)(*([0] * self.action_size))
        self._physics.getPhysicsRanges(state_min, state_max, action_min, action_max)
        
        self.state_min = np.array([ state_min[i] if abs(state_min[i]) < 9999.0 else -np.inf      for i in range(self.state_size) ])
        self.state_max = np.array([ state_max[i] if abs(state_max[i]) < 9999.0 else np.inf       for i in range(self.state_size) ])
        self.action_min = np.array([ action_min[i] if abs(action_min[i]) < 9999.0 else -np.inf   for i in range(self.action_size) ])
        self.action_max = np.array([ action_max[i] if abs(action_max[i]) < 9999.0 else np.inf    for i in range(self.action_size) ])


    def reset(self):
        out_state = (ctypes.c_float * self.state_size)(*([0] * self.state_size))
        self._physics.resetPhysics(out_state)
        result_state = [ out_state[i] for i in range(self.state_size) ]
        return result_state

    def step(self, action):
        out_state = (ctypes.c_float * self.state_size)(*([0] * self.state_size))
        out_reward = (ctypes.c_float * 1)(*[0.0])
        action = (ctypes.c_float * self.action_size)(*action)

        done = self._physics.updatePhysics(action, out_state, out_reward)

        result_state = [ out_state[i] for i in range(self.state_size) ]

        return result_state, out_reward[0], done != 0


# https://blog.paperspace.com/creating-custom-environments-openai-gym/
# Simple example: https://github.com/openai/gym/blob/master/gym/envs/classic_control/pendulum.py

class LocoEnv(gym.Env):
    metadata = {'render.modes': ['human']}

    def __init__(self):
        super(LocoEnv, self).__init__()

        self.dll = PhysicsDLL()

        self.observation_space = spaces.Box(np.float32(self.dll.state_min), np.float32(self.dll.state_max))
        self.action_space = spaces.Box(np.float32(self.dll.action_min), np.float32(self.dll.action_max))

        self.reset()



    def step(self, action):
        new_state, reward, done = self.dll.step(action)
        return np.array(new_state, dtype=np.float32), reward, done, {}

    def reset(self):
        state = self.dll.reset()
        return np.array(state, dtype=np.float32)

    def render(self, mode='human', close=False):
        # Not implemented.
        pass
    


# For testing only.

def main():
    dll = PhysicsDLL()
    print(dll.action_size)

    action = list(range(dll.action_size))
    state, reward, done = dll.step(action)
    print(state, reward, done)

    state = dll.reset()
    print(state)

if __name__ == "__main__":
    main()

