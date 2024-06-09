#include "messages.h"

const char nano_msg_list[2][NANO_MSG_NUM][256] = {
	{
		"Hello! Welcome to nanotodon!\n",
		"First, ",
		"Please tell me the server where you live.\n(https://[please enter this part]/)\n",
		"Something is wrong,\nlet me start over from first.\n",
		"Next, I will do application authentication.\n",
		"Please access to following URL, then after authorization, please input displayed authorization code.\n",
		"Something is wrong.\nIs authorization code right?\nLet me start over from first.\n",
		"It\'s done!\nPlease enjoy nanotodon life!\n",
	},
	{
		"はじめまして！ようこそnaotodonへ!\n",
		"最初に、",
		"あなたのいるサーバーを教えてね。\n(https://[ここを入れてね]/)\n",
		"何かがおかしいみたいだよ。\nもう一度やり直すね。\n",
		"次に、アプリケーションの認証をするよ。\n",
		"下に表示されるURLにアクセスして、承認をしたら表示されるコードを入力してね。\n",
		"何かがおかしいみたいだよ。\n入力したコードはあっているかな？\nもう一度やり直すね。\n",
		"これでおしまい!\nnanotodonライフを楽しんでね!\n",
	}
};
