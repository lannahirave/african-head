# Опис файлів проєкту

## Збірка та запуск

- `CMakeLists.txt` — конфігурація CMake: збірка застосунку `african-head`, підключення OpenCV/OpenMP, копіювання моделей і текстур у build-директорію.
- `build_and_run.sh` — короткий скрипт для збірки (`cmake --build build -j`) і запуску програми з каталогу `build`.

## Основний код рендерера

- `main.cpp` — точка входу, цикл рендеру, обробка клавіш (`P`, `S`, `ESC`), ініціалізація камери/світла/проєкції, запуск растеризації.
- `geometry.h` — власна математична бібліотека: вектори/матриці, детермінант, обернена матриця, нормалізація, векторний добуток, матриці трансформацій.
- `renderer.h` — інтерфейси й глобальні стани рендерера: шейдерний інтерфейс `IShader`, матриці (`ModelView`, `Perspective`, `Viewport`), Z-буфер, API растеризатора.
- `renderer.cpp` — реалізація рендер-пайплайну: `lookat`, проєкції, viewport, ініціалізація/очищення Z-буфера, Брезенгем, растеризація трикутників.
- `phong_shader.h` — оголошення `PhongShader` (vertex + fragment), змінні для інтерполяції атрибутів і normal mapping.
- `phong_shader.cpp` — реалізація шейдера Фонга: UV/нормалі, тангентний базис, ambient + diffuse + specular освітлення, фінальний колір пікселя.
- `model.h` — клас `Model`: інтерфейс завантаження OBJ, доступ до вершин/UV/нормалей, семплінг diffuse/normal/specular карт.
- `model.cpp` — реалізація парсера OBJ і завантажувача TGA, зберігання мешу та текстур, вибірка текстурних значень за UV.

## Контекстні документи

- `project_context.md` — загальний опис проєкту, реалізованих пунктів і пайплайну рендерингу.
- `light_context.md` — пояснення системи координат і напряму світла для швидкого налаштування освітлення.

## 3D-моделі

- `models/african_head.obj` — основна геометрія голови.
- `models/african_head_eye_inner.obj` — внутрішня частина очей.
- `models/african_head_eye_outer.obj` — зовнішня частина очей.

## Текстури

- `textures/african_head_diffuse.tga` — дифузна текстура голови.
- `textures/african_head_nm_tangent.tga` — normal map голови (tangent space).
- `textures/african_head_spec.tga` — specular map голови.
- `textures/african_head_eye_inner_diffuse.tga` — дифузна текстура внутрішньої частини очей.
- `textures/african_head_eye_inner_nm_tangent.tga` — normal map внутрішньої частини очей.
- `textures/african_head_eye_inner_spec.tga` — specular map внутрішньої частини очей.
- `textures/african_head_eye_outer_diffuse.tga` — дифузна текстура зовнішньої частини очей.
- `textures/african_head_eye_outer_nm_tangent.tga` — normal map зовнішньої частини очей.
- `textures/african_head_eye_outer_spec.tga` — specular map зовнішньої частини очей.
