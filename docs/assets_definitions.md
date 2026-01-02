# Определение ресурсов (assets)

Документ описывает формат файлов ресурсов и правила их адресации на стороне сервера.
Описание основано на загрузчиках из `Src/Server/AssetsManager.hpp` и связанных структурах
подготовки (`PreparedNodeState`, `PreparedModel`, `PreparedGLTF`).

## Общая схема
- Ресурсы берутся из списка папок `AssetsRegister::Assets` (от последнего мода к первому).
  Первый найденный ресурс по пути имеет приоритет.
- Переопределения через `AssetsRegister::Custom` имеют более высокий приоритет.
- Адрес ресурса состоит из `domain` и `key`.
  `domain` — имя папки в assets, `key` — относительный путь внутри папки типа ресурса.
- Обработанные ресурсы сохраняются в `server_cache/assets`.

## Дерево папок
```
assets/
  <domain>/
    nodestate/   *.json
    model/       *.json | *.gltf | *.glb
    texture/     *.png | *.jpg (jpeg)
    particle/    (загрузка из файлов пока не реализована)
    animation/   (загрузка из файлов пока не реализована)
    sound/       (загрузка из файлов пока не реализована)
    font/        (загрузка из файлов пока не реализована)
```

Пример: `assets/core/nodestate/stone.json` имеет `domain=core`, `key=stone.json`.
При обращении к nodestate из логики нод используется ключ без суффикса `.json`
(сервер дописывает расширение автоматически).

## Nodestate (JSON)
Файл nodestate — это JSON-объект, где ключи — условия, а значения — описание модели
или список вариантов моделей.

### Условия
Условие — строковое выражение. Поддерживаются:
- числа, `true`, `false`
- переменные: `state` или `state:value` (двоеточие — часть имени)
- операторы: `+ - * / %`, `!`, `&`, `|`, `< <= > >= == !=`
- скобки

Пустая строка условия трактуется как `true`.

### Формат варианта модели
Объект варианта:
- `model`: строка `domain:key` **или** массив объектов моделей
- `weight`: число (вес при случайном выборе), по умолчанию `1`
- `uvlock`: bool (используется для векторных моделей; для одиночной модели игнорируется)
- `transformations`: массив строк `"key=value"` для трансформаций

Если `model` — строка, это одиночная модель.
Если `model` — массив, это векторная модель: набор объектов вида:
```
{ "model": "domain:key", "uvlock": false, "transformations": ["x=0", "ry=1.57"] }
```
Для векторной модели также могут задаваться `uvlock` и `transformations` на верхнем уровне
(они применяются к группе).

Трансформации поддерживают ключи:
`x`, `y`, `z`, `rx`, `ry`, `rz` (сдвиг и поворот).

Домен в строке `domain:key` можно опустить — тогда используется домен файла nodestate.

### Пример
```json
{
  "": { "model": "core:stone" },
  "variant == 1": [
    { "model": "core:stone_alt", "weight": 2 },
    { "model": "core:stone_alt_2", "weight": 1, "transformations": ["ry=1.57"] }
  ],
  "facing:north": {
    "model": [
      { "model": "core:stone", "transformations": ["ry=3.14"] },
      { "model": "core:stone_detail", "transformations": ["x=0.5"] }
    ],
    "uvlock": true
  }
}
```

## Model (JSON)
Формат описывает геометрию и текстуры.

### Верхний уровень
- `gui_light`: строка (сейчас используется только `default`)
- `ambient_occlusion`: bool
- `display`: объект с наборами `rotation`/`translation`/`scale` (все — массивы из 3 чисел)
- `textures`: объект `name -> string` (ссылка на текстуру или pipeline)
- `cuboids`: массив геометрических блоков
- `sub_models`: массив подмоделей

### Текстуры
В `textures` значение:
- либо строка `domain:key` (прямая ссылка на текстуру),
- либо pipeline-строка, начинающаяся с `tex` (компилируется `TexturePipelineProgram`).

Если домен не указан, используется домен файла модели.

### Cuboids
Элемент `cuboids`:
- `shade`: bool (по умолчанию `true`)
- `from`: `[x, y, z]`
- `to`: `[x, y, z]`
- `faces`: объект граней (`down|up|north|south|west|east`)
- `transformations`: массив `"key=value"` (ключи как у nodestate)

Грань (`faces.<name>`) может содержать:
- `uv`: `[u0, v0, u1, v1]`
- `texture`: строка (ключ из `textures`)
- `cullface`: `down|up|north|south|west|east`
- `tintindex`: int
- `rotation`: int16

### Sub-models
`sub_models` допускает:
- строку `domain:key`
- объект `{ "model": "domain:key", "scene": 0 }`
- объект `{ "path": "domain:key", "scene": 0 }`

Поле `scene` опционально.

### Пример
```json
{
  "ambient_occlusion": true,
  "textures": {
    "all": "core:stone"
  },
  "cuboids": [
    {
      "from": [0, 0, 0],
      "to": [16, 16, 16],
      "faces": {
        "north": { "uv": [0, 0, 16, 16], "texture": "#all" }
      }
    }
  ],
  "sub_models": [
    "core:stone_detail",
    { "model": "core:stone_variant", "scene": 1 }
  ]
}
```

## Model (glTF / GLB)
Файлы моделей могут быть:
- `.gltf` (JSON glTF)
- `.glb` (binary glTF)

Оба формата конвертируются в `PreparedGLTF`.

## Texture
Поддерживаются только PNG и JPEG.
Формат определяется по сигнатуре файла.

## Прочие типы ресурсов
Для `particle`, `animation`, `sound`, `font` загрузка из файловой системы
в серверном загрузчике пока не реализована (`std::unreachable()`), но возможна
регистрация из Lua через `path` (сырые бинарные данные).
