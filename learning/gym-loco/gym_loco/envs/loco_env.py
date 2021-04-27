import gym
from gym import error, spaces, utils
from gym.utils import seeding
import numpy as np
import ctypes

class PhysicsDLL() :
    def __init__(self):
        self._physics = ctypes.CDLL('bin/Release_x86_64/Physics-Lib.dll')

        print(self._physics)
        
        self._physics.updatePhysics.argtypes = (ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float))
        self._physics.resetPhysics.argtypes = (ctypes.POINTER(ctypes.c_float),)

        self.state_size = self._physics.getPhysicsStateSize()
        self.action_size = self._physics.getPhysicsActionSize()

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

        obs_high = np.array([np.inf] * self.dll.state_size)
        act_high = np.array([1] * self.dll.action_size)

        self.observation_space = spaces.Box(-obs_high, obs_high, dtype=np.float32)
        self.action_space = spaces.Box(-act_high, act_high, dtype=np.float32)

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

