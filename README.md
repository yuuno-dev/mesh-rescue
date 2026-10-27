# mesh-rescue

**English | [日本語](#日本語)**

## English

When a big earthquake hits, phone lines and the internet can go down. Then nobody knows who is safe and who needs help.

This is a small prototype for that situation. Nearby devices find each other and share a simple status, with no server and no internet. Later, messages will hop from device to device, so even someone far away can be reached.

I'm a student in Japan learning C and networking, and I'm building this to understand how mesh networks really work. It's not finished. I'm adding to it step by step.

### What works now (Step 1)

- Each program sends a small HELLO message over UDP every 3 seconds
- Each program keeps a list of the people it heard recently
- Status: OK / NEED HELP / TRAPPED (press 1, 2, 3)
- Anyone who goes quiet for 10 seconds drops off the list



### Plan

1. Discover nearby devices (done)
2. Relay messages with a hop limit and measure delivery rate and delay
3. Move to ESP32 boards, still in C
4. Simple phone page for status buttons
5. Field tests: distance, walls, battery

---

## 日本語

大きな地震が起きると、携帯電話の回線やインターネットが使えなくなることがあります。そうなると、誰が無事で、誰が助けを必要としているのか分からなくなります。

このプロジェクトは、サーバーもインターネットも使わずに、近くの機器どうしが見つけ合って、簡単な状態を共有する試作です。今後は、メッセージを機器から機器へ中継して、遠くの人にも届くようにしたいと考えています。

日本の学校でCとネットワークを勉強している学生で、メッシュネットワークの仕組みを学ぶために作っています。まだ途中ですが、少しずつ進めています。

### 今できること(ステップ1)

- 各プログラムが3秒ごとにUDPで「HELLO」を送信
- 最近聞こえた人の一覧を表示
- 状態は OK / 助けが必要 / 閉じ込められた(キー 1、2、3)
- 10秒間聞こえない人は一覧から消える



### 今後の予定

1. 近くの機器を見つける(完了)
2. ホップ数つきのメッセージ中継と、到達率・遅延の測定
3. ESP32ボードへ移行(引き続きC言語)
4. 状態ボタンのスマホ用ページ
5. 屋外テスト(距離、壁、電池)
