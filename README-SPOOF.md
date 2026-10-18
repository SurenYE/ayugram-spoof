# AyuGram Spoof — клиент Telegram с «панелью подделок»

Модификация [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) (GPL-3.0, форк
Telegram Desktop для ПК: Windows / Linux / macOS).

## Что уже умеет база (AyuGram, включено по умолчанию)

- **Все удалённые сообщения и чаты сохраняются** (anti-recall, `_saveDeletedMessages = true`).
- **Исчезающие медиа сохраняются**: view-once фото, «сгорающие» кружки, голосовые и видео
  не пропадают после просмотра/удаления.
- Локальная история сообщений (`_saveMessagesHistory = true`).

## Новая фича: панель подделок (spoof panel)

Позволяет сохранить **сколько угодно** голосовых сообщений и видеокружков (без лимита)
и отправлять их в переписке так, будто вы **только что их записали**:

1. **Добавление.** Правый клик по входящему голосовому сообщению или кружку →
   «Добавить в панель подделок». Если файл ещё не скачан, клиент докачает его сам.
   Файл (и превью для кружка) копируется в папку клиента и переживает перезапуск.
2. **Панель.** Главное меню (кнопка-«гамбургер» слева сверху) → «Панель подделок».
   В панели сеткой показаны все сохранённые файлы: кружки — с круглым превью-кадром,
   голосовые — с волной; у каждого видна длительность и тип.
3. **Предпросмотр.** Клик по элементу панели открывает окно просмотра: кружок
   проигрывается в круглом плеере (клик — пауза/продолжить), голосовой — с кнопкой
   воспроизведения и прогресс-баром с таймером.
4. **Отправка.** В окне предпросмотра кнопка **«Send»**:
   - собеседник видит статус **«записывает видеосообщение»** (для кружка) или
     **«записывает голосовое сообщение»** (для голосового) **ровно столько секунд,
     какова длительность файла**;
   - затем файл отправляется **как новое сообщение** (не пересылка) в текущий открытый чат.
5. **Удаление.** Правый клик по элементу панели → «Удалить из панели».
   Кнопка «Очистить всё» удаляет все записи.

Файлы панели лежат в `tdata/ayu/spoof_panel/` внутри рабочей папки клиента.

## Сборка (Linux, через podman/docker)

```bash
git clone --recursive https://github.com/AyuGram/AyuGramDesktop.git
cd AyuGramDesktop

# 1) Собрать libtlottie (Rust-зависимость Ayugram) и положить в образ-надстройку:
#    см. tlottie-build/Containerfile рядом с репозиторием
# 2) Сборка:
podman run --rm --userns=keep-id \
    -v "$PWD:/usr/src/tdesktop:Z" \
    -e CONFIG=Debug localhost/ayugram:centos_env \
    /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
    -D TDESKTOP_API_ID=2040 \
    -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627
```

Готовый бинарник: `out/Debug/AyuGram` (Qt статически слинкован — запускается прямо на хосте).
Собранный экземпляр уже ужат `strip` (6,5 ГБ → ~479 МБ; это отладочная сборка, но рабочая).

## Запуск (Linux)

```bash
# обычный Wayland/X11-запуск:
/home/sska/tgbuild/AyuGramDesktop/out/Debug/AyuGram --workdir /home/sska/tgbuild/ayuwork
# если на Wayland есть проблемы с рендером — форсить X11-бэкенд:
QT_QPA_PLATFORM=xcb /home/sska/tgbuild/AyuGramDesktop/out/Debug/AyuGram --workdir /home/sska/tgbuild/ayuwork
```

Первый запуск — окно входа в Telegram (нужен номер телефона). После входа:
- **anti-recall / исчезающие медиа** работают сразу (наследие AyuGram, по умолчанию включено);
- **панель подделок** — в меню-гамбургере слева сверху («Панель подделок»);
- добавить кружок/голосовое в панель — правый клик по входящему голосовому/кружку →
  «Добавить в панель подделок» (файл докачается сам, если ещё не скачан).

## Что изменено в коде (список файлов)

- `Telegram/SourceFiles/ayu/spoof/spoof_storage.{h,cpp}` — хранилище списка (JSON-индекс + файлы).
- `Telegram/SourceFiles/ayu/spoof/spoof_sender.{h,cpp}` — статус «записывает…» + отправка как нового сообщения.
- `Telegram/SourceFiles/ayu/ui/spoof_panel/spoof_panel_box.{h,cpp}` — панель с сеткой превью.
- `Telegram/SourceFiles/ayu/ui/spoof_panel/spoof_preview_box.{h,cpp}` — окно предпросмотра (ffmpeg-стриминг tdesktop) с кнопкой отправки.
- `Telegram/SourceFiles/ayu/ui/context_menu/context_menu.{h,cpp}` — пункт «Добавить в панель подделок».
- `Telegram/SourceFiles/history/view/history_view_context_menu.cpp` — подключение пункта.
- `Telegram/SourceFiles/window/window_main_menu.cpp` — вход в панель из главного меню.
- `Telegram/Resources/langs/lang.strings` — тексты.
- `Telegram/SourceFiles/ayu/ui/ayu_styles.style` — размеры/шрифт панели.
- `Telegram/CMakeLists.txt` — регистрация новых файлов.
