from gym.envs.registration import register

register(
    id='loco-v0',
    entry_point='gym_loco.envs:LocoEnv',
)