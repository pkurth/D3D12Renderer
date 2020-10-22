#pragma once

enum kb_button
{
	button_0, button_1, button_2, button_3, button_4, button_5, button_6, button_7, button_8, button_9,
	button_a, button_b, button_c, button_d, button_e, button_f, button_g, button_h, button_i, button_j,
	button_k, button_l, button_m, button_n, button_o, button_p, button_q, button_r, button_s, button_t,
	button_u, button_v, button_w, button_x, button_y, button_z,
	button_space, button_enter, button_shift, button_alt, button_tab, button_ctrl, button_esc,
	button_up, button_down, button_left, button_right,
	button_backspace, button_delete,

	button_f1, button_f2, button_f3, button_f4, button_f5, button_f6, button_f7, button_f8, button_f9, button_f10, button_f11, button_f12,

	button_count, button_unknown
};

struct button_state
{
	bool isDown;
	bool wasDown;
};

struct keyboard_input
{
	button_state buttons[button_count];
};

struct mouse_input
{
	button_state left;
	button_state right;
	button_state middle;
	float scroll;

	int x;
	int y;

	float relX;
	float relY;

	int dx;
	int dy;

	float reldx;
	float reldy;
};

struct user_input
{
	button_state keyboard[button_count];
	mouse_input mouse;
};

inline bool isDown(const user_input& input, kb_button buttonID)
{
	return input.keyboard[buttonID].isDown;
}

inline bool isUp(const user_input& input, kb_button buttonID)
{
	return !input.keyboard[buttonID].isDown;
}

inline bool buttonDownEvent(const button_state& button)
{
	return button.isDown && !button.wasDown;
}

inline bool buttonUpEvent(const button_state& button)
{
	return !button.isDown && button.wasDown;
}

inline bool buttonDownEvent(const user_input& input, kb_button buttonID)
{
	return buttonDownEvent(input.keyboard[buttonID]);
}

inline bool buttonUpEvent(const user_input& input, kb_button buttonID)
{
	return buttonUpEvent(input.keyboard[buttonID]);
}
