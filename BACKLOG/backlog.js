"use strict";

// ============================================================================
// НАЧАЛО ДАННЫХ БЭКЛОГА — агентам редактировать BACKLOG_DATA в этом блоке.
// Это единственный источник задач, категорий, приоритетов и статусов страницы.
// Формат: BACKLOG/README.md; план и приоритеты: AGENTS.md. Обновите index.html.
// ============================================================================
const BACKLOG_DATA = {
  "version": 1,
  "categories": [
    {
      "id": "vehicle-controls",
      "title": "Управление транспортом"
    },
    {
      "id": "vehicle-calibration",
      "title": "Настройка транспорта"
    },
    {
      "id": "vehicle-content",
      "title": "Модели и интерьеры транспорта"
    },
    {
      "id": "story",
      "title": "Сюжет: проверка проходимости"
    },
    {
      "id": "missions",
      "title": "Задания и взаимодействия с миром"
    },
    {
      "id": "weapons",
      "title": "Оружие и бой"
    },
    {
      "id": "player",
      "title": "Персонаж и перемещение"
    },
    {
      "id": "hud",
      "title": "HUD и интерфейс"
    },
    {
      "id": "audio",
      "title": "Звук"
    },
    {
      "id": "graphics",
      "title": "Графика и камера"
    },
    {
      "id": "tooling",
      "title": "Инструменты"
    },
    {
      "id": "bugs",
      "title": "Ошибки и регрессии"
    }
  ],
  "tasks": [
    {
      "id": 9,
      "title": "Проверка положения руля на всех моделях автомобилей",
      "file": "09-proverka-polozheniya-rulya-na-modelyakh-avtomobiley.md",
      "categories": [
        "vehicle-calibration",
        "vehicle-controls",
        "tooling"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 25,
      "title": "Бензопила: двуручный хват и активация нажатием",
      "file": "25-benzopila-dvuruchnyy-khvat-i-aktivatsiya.md",
      "categories": [
        "weapons"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 27,
      "title": "Корректировка дефолтных углов оружия",
      "file": "27-korrektirovka-defoltnykh-uglov-oruzhiya.md",
      "categories": [
        "weapons",
        "bugs"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 35,
      "title": "Автоматическое восстановление здоровья",
      "file": "35-avtomaticheskoe-vosstanovlenie-zdorovya.md",
      "categories": [
        "player"
      ],
      "priority": "low",
      "status": "open"
    },
    {
      "id": 37,
      "title": "Управление: убрать посадку в авто по кнопке и спринты на триггерах",
      "file": "37-upravlenie-ubrat-posadku-po-knopke-i-sprinty-na-triggerakh.md",
      "categories": [
        "vehicle-controls",
        "player"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 39,
      "title": "Убрать тень CJ",
      "file": "39-ubrat-ten-cj.md",
      "categories": [
        "graphics",
        "player"
      ],
      "priority": "low",
      "status": "open"
    },
    {
      "id": 40,
      "title": "Ручная перезарядка оружия",
      "file": "40-ruchnaya-perezaryadka-oruzhiya.md",
      "categories": [
        "weapons"
      ],
      "priority": "low",
      "status": "open"
    },
    {
      "id": 41,
      "title": "Рекрутинг в банду через жесты",
      "file": "41-rekruting-v-bandu-cherez-zhesty.md",
      "categories": [
        "player",
        "missions"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 44,
      "title": "Отдельный тестовый спавнер мотоциклов",
      "file": "44-otdelnyy-testovyy-spavner-mototsiklov.md",
      "categories": [
        "vehicle-calibration",
        "vehicle-controls",
        "tooling"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 48,
      "title": "Выход из машины на скорости с уроном",
      "file": "48-vykhod-iz-mashiny-na-skorosti.md",
      "categories": [
        "vehicle-controls",
        "player"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 49,
      "title": "Исследовать скрытие окон в машине",
      "file": "49-skrytie-okon-v-mashine.md",
      "categories": [
        "vehicle-content",
        "graphics"
      ],
      "priority": "low",
      "status": "open"
    },
    {
      "id": 50,
      "title": "Исследовать Drive By с выглядыванием из окна",
      "file": "50-drive-by-iz-okna.md",
      "categories": [
        "vehicle-controls",
        "weapons",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 51,
      "title": "Поведение хоумиз при стрельбе и розыске в машине",
      "file": "51-khoumiz-strelba-i-rozysk-v-mashine.md",
      "categories": [
        "vehicle-controls",
        "missions"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 57,
      "title": "Посадка и угон лодок",
      "file": "57-posadka-i-ugon-lodok.md",
      "categories": [
        "vehicle-controls",
        "player",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 62,
      "title": "Лестница Dozer",
      "file": "62-lestnitsa-dozera.md",
      "categories": [
        "vehicle-controls",
        "player",
        "story"
      ],
      "priority": "critical",
      "status": "open"
    },
    {
      "id": 68,
      "title": "Исправить ухудшение звука",
      "file": "68-ispravit-ukhudshenie-zvuka.md",
      "categories": [
        "audio",
        "bugs"
      ],
      "priority": "high",
      "status": "in_progress"
    },
    {
      "id": 73,
      "title": "Отдельные точки дула для моделей оружия",
      "file": "73-otdelnye-tochki-dula-oruzhiya.md",
      "categories": [
        "weapons"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 74,
      "title": "Перекраска машины без передачи управления VR-камерой",
      "file": "74-perekraska-bez-peredachi-vr-kamery.md",
      "categories": [
        "vehicle-controls",
        "graphics",
        "bugs"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 75,
      "title": "Отдельная настройка счётчика патронов для каждого оружия",
      "file": "75-nastroyka-schetchika-patronov-dlya-kazhdogo-oruzhiya.md",
      "categories": [
        "weapons",
        "hud",
        "tooling"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 77,
      "title": "Пространственная настройка элементов HUD",
      "file": "77-prostranstvennaya-nastroyka-elementov-hud.md",
      "categories": [
        "hud",
        "tooling"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 79,
      "title": "Убрать зеркалирование модели оружия между руками",
      "file": "79-ubrat-zerkalirovanie-modeli-oruzhiya.md",
      "categories": [
        "weapons",
        "graphics",
        "bugs"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 82,
      "title": "Скрывать VR-элементы, когда камера не принадлежит CJ",
      "file": "82-skrytie-vr-elementov-pri-chuzhoy-kamere.md",
      "categories": [
        "hud",
        "graphics",
        "bugs"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 83,
      "title": "Исправить задержку падения NPC при сбивании с ног",
      "file": "83-zaderzhka-padeniya-npc-v-boyu.md",
      "categories": [
        "weapons",
        "bugs"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 84,
      "title": "Настройка угла наклона автомобильного радара через дебаг-меню",
      "file": "84-naklon-avtomobilnogo-radara.md",
      "categories": [
        "vehicle-calibration",
        "hud",
        "tooling"
      ],
      "priority": "critical",
      "status": "in_progress"
    },
    {
      "id": 85,
      "title": "Скрывать крутилку радио на транспорте без радио",
      "file": "85-skrytie-krutilki-na-transporte-bez-radio.md",
      "categories": [
        "vehicle-controls",
        "audio",
        "bugs"
      ],
      "priority": "critical",
      "status": "in_progress"
    },
    {
      "id": 86,
      "title": "Следование камеры за CJ и запрет стрельбы при сбивании с ног",
      "file": "86-kamera-i-strelba-pri-sbivanii-cj.md",
      "categories": [
        "player",
        "weapons",
        "graphics",
        "bugs"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 87,
      "title": "Спринт с одноручным оружием в двух руках или двумя оружиями",
      "file": "87-sprint-s-oruzhiem-v-obeikh-rukakh.md",
      "categories": [
        "player",
        "weapons",
        "bugs"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 88,
      "title": "Ускорение спринта движениями рук",
      "file": "88-uskorenie-sprinta-dvizheniyami-ruk.md",
      "categories": [
        "player"
      ],
      "priority": "low",
      "status": "open"
    },
    {
      "id": 89,
      "title": "Настройка подбора оружия с земли без ложных срабатываний",
      "file": "89-nastroyka-podbora-oruzhiya-s-zemli.md",
      "categories": [
        "weapons",
        "bugs"
      ],
      "priority": "high",
      "status": "in_progress"
    },
    {
      "id": 90,
      "title": "Физика и коллизии бросаемого оружия",
      "file": "90-fizika-i-kollizii-brosaemogo-oruzhiya.md",
      "categories": [
        "weapons",
        "bugs"
      ],
      "priority": "low",
      "status": "open"
    },
    {
      "id": 91,
      "title": "Поза CJ и коллизия получения урона по позе HMD",
      "file": "91-poza-cj-i-kolliziya-urona-po-hmd.md",
      "categories": [
        "player",
        "weapons",
        "graphics"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 92,
      "title": "Рассинхрон руля и направления колеса на двухколёсном транспорте",
      "file": "92-rassinkhron-rulya-na-dvukhkolesnom-transporte.md",
      "categories": [
        "vehicle-controls",
        "bugs"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 93,
      "title": "Регрессия: не срабатывает жест блока",
      "file": "93-ne-srabatyvaet-zhest-bloka.md",
      "categories": [
        "weapons",
        "bugs"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 94,
      "title": "Наклон вперёд/назад на квадроцикле с вибрацией и позой кистей",
      "file": "94-naklon-vpered-nazad-na-kvadrotsikle.md",
      "categories": [
        "vehicle-controls"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 95,
      "title": "Исправить отсутствие поворота модели CJ в дебаг-режиме",
      "file": "95-povorot-modeli-cj-v-debag-rezhime.md",
      "categories": [
        "player",
        "graphics",
        "tooling",
        "bugs"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 96,
      "title": "Заменить модель ключа зажигания на созданную в Blender",
      "file": "96-model-klyucha-zazhiganiya-v-blender.md",
      "categories": [
        "vehicle-content",
        "vehicle-controls"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 98,
      "title": "Телефон в машине сутенёра для запуска миссии",
      "file": "98-telefon-v-mashine-sutenera.md",
      "categories": [
        "vehicle-controls",
        "missions"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 100,
      "title": "Виброобратная связь при рулении: холодно/горячо и эксперимент с PCM",
      "file": "100-vibrootklik-pri-rulenii.md",
      "categories": [
        "vehicle-controls"
      ],
      "priority": "low",
      "status": "in_progress"
    },
    {
      "id": 101,
      "title": "Работающий фотоаппарат с экраном и сохранением снимков через API",
      "file": "101-fotoapparat-ekran-i-sokhranenie-snimkov.md",
      "categories": [
        "weapons",
        "hud",
        "missions",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 102,
      "title": "Иммерсивные автоматы с газировкой и снеками",
      "file": "102-immersivnye-torgovye-avtomaty.md",
      "categories": [
        "player",
        "missions"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 103,
      "title": "Обратная связь при касании часов",
      "file": "103-obratnaya-svyaz-kasaniya-chasov.md",
      "categories": [
        "hud"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 104,
      "title": "Разделы проекции часов и статистика игрока",
      "file": "104-razdely-proektsii-chasov-i-statistika.md",
      "categories": [
        "hud",
        "player"
      ],
      "priority": "low",
      "status": "open"
    },
    {
      "id": 105,
      "title": "Исправить прозрачность иконок здоровья и брони на часах",
      "file": "105-alfa-ikonok-zdorovya-i-broni-chasov.md",
      "categories": [
        "hud",
        "graphics",
        "bugs"
      ],
      "priority": "low",
      "status": "open"
    },
    {
      "id": 106,
      "title": "Регрессия: перестал работать автомобильный сигнал",
      "file": "106-ne-rabotaet-avtomobilnyy-signal.md",
      "categories": [
        "vehicle-controls",
        "audio",
        "bugs"
      ],
      "priority": "critical",
      "status": "open"
    },
    {
      "id": 107,
      "title": "Таксометр для запуска задания таксиста",
      "file": "107-taksometr-dlya-zadaniya-taksista.md",
      "categories": [
        "vehicle-controls",
        "hud",
        "missions"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 108,
      "title": "Интерьеры спецтехники и наземного транспорта",
      "file": "108-interiery-spetstekhniki-i-nazemnogo-transporta.md",
      "categories": [
        "vehicle-content",
        "vehicle-calibration",
        "graphics",
        "hud",
        "story"
      ],
      "priority": "critical",
      "status": "open"
    },
    {
      "id": 109,
      "title": "Проверка и создание интерьеров авиации",
      "file": "109-interiery-aviatsii.md",
      "categories": [
        "vehicle-content",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 110,
      "title": "Рычаг управления кузовом Dumper",
      "file": "110-rychag-kuzova-dumper.md",
      "categories": [
        "vehicle-controls",
        "missions"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 111,
      "title": "Рычаг управления платформой Packer",
      "file": "111-rychag-platformy-packer.md",
      "categories": [
        "vehicle-controls"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 112,
      "title": "Управление барабаном Cement Truck",
      "file": "112-upravlenie-barabanom-cement-truck.md",
      "categories": [
        "vehicle-controls"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 114,
      "title": "Управление сиреной служебного транспорта",
      "file": "114-upravlenie-sirenoy.md",
      "categories": [
        "vehicle-controls",
        "audio"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 115,
      "title": "Ночные кражи на Boxburg: запуск и погрузка добычи",
      "file": "115-nochnye-krazhi-boxburg.md",
      "categories": [
        "vehicle-controls",
        "missions",
        "player"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 116,
      "title": "Кабина локомотива и задания Freight Train",
      "file": "116-lokomotivy-i-freight-train.md",
      "categories": [
        "vehicle-controls",
        "vehicle-content",
        "missions"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 117,
      "title": "Курьерские задания: броски пакетов с двухколёсного транспорта",
      "file": "117-kuryerskie-zadaniya-i-pakety.md",
      "categories": [
        "vehicle-controls",
        "missions"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 118,
      "title": "Дым Stuntplane и распыление Cropduster",
      "file": "118-dym-i-raspylenie-samoletov.md",
      "categories": [
        "vehicle-controls",
        "graphics"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 119,
      "title": "Прыжок велосипеда движением рук",
      "file": "119-pryzhok-velosipeda.md",
      "categories": [
        "vehicle-controls",
        "player"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 120,
      "title": "Бонусные прыжок и нитро Taxi и Cabbie",
      "file": "120-bonusnye-pryzhok-i-nitro-taksi.md",
      "categories": [
        "vehicle-controls",
        "missions"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 121,
      "title": "Физическое управление четырьмя колёсами Monster",
      "file": "121-podrulivanie-monster.md",
      "categories": [
        "vehicle-controls",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 122,
      "title": "Сцепление и буксировка прицепов",
      "file": "122-stseplenie-i-buksirovka-pritsepov.md",
      "categories": [
        "vehicle-controls",
        "missions",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 123,
      "title": "Управление грузовой рампой Andromada",
      "file": "123-gruzovaya-rampa-andromada.md",
      "categories": [
        "vehicle-controls",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 124,
      "title": "Управление прожектором Police Maverick",
      "file": "124-prozhektor-police-maverick.md",
      "categories": [
        "vehicle-controls",
        "graphics"
      ],
      "priority": "medium",
      "status": "open"
    },
    {
      "id": 125,
      "title": "Физическое управление магнитной подвеской Leviathan",
      "file": "125-magnitnaya-podveska-leviathan.md",
      "categories": [
        "vehicle-controls",
        "missions",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 126,
      "title": "Физическое управление кранами",
      "file": "126-fizicheskoe-upravlenie-kranami.md",
      "categories": [
        "vehicle-controls",
        "missions"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 127,
      "title": "Иммерсивный пульт управления RC-техникой",
      "file": "127-pult-upravleniya-rc-tekhnikoy.md",
      "categories": [
        "vehicle-controls",
        "missions",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 128,
      "title": "Специальные органы управления и вооружение авиации",
      "file": "128-spetsialnye-organy-upravleniya-aviatsii.md",
      "categories": [
        "vehicle-controls",
        "weapons",
        "story"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 129,
      "title": "Физическое управление башней и пушкой Rhino",
      "file": "129-fizicheskoe-upravlenie-bashney-rhino.md",
      "categories": [
        "vehicle-controls",
        "weapons",
        "hud",
        "missions"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 130,
      "title": "Служебные задания на HPV1000, Rhino и Hunter",
      "file": "130-sluzhebnye-zadaniya-hpv1000-rhino-hunter.md",
      "categories": [
        "vehicle-controls",
        "missions"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 131,
      "title": "Показывать грипы мотоциклов и квадроциклов в дебаг-режиме",
      "file": "131-vidimost-gripov-v-debag-rezhime.md",
      "categories": [
        "vehicle-calibration",
        "vehicle-controls",
        "tooling"
      ],
      "priority": "high",
      "status": "open"
    },
    {
      "id": 132,
      "title": "Смещение рук на руле при авариях",
      "file": "132-smeshchenie-ruk-na-rule-pri-avariyakh.md",
      "categories": [
        "vehicle-controls",
        "bugs"
      ],
      "priority": "critical",
      "status": "in_progress"
    }
  ]
};
// КОНЕЦ ДАННЫХ БЭКЛОГА. Ниже — логика страницы; для ведения задач её не менять.
// ============================================================================
(() => {
  const priorities = {
    critical: { label: "Критический", rank: 0 },
    high: { label: "Высокий", rank: 1 },
    medium: { label: "Средний", rank: 2 },
    low: { label: "Низкий", rank: 3 },
    none: { label: "Не задан", rank: 4 },
  };
  const statuses = { open: "Открыта", in_progress: "В работе" };
  const has = (object, key) => Object.hasOwn(object, key);

  function validateIndex(data) {
    const fail = (message) => { throw new Error(`Некорректный BACKLOG_DATA: ${message}`); };
    if (!data || data.version !== 1 || !Array.isArray(data.categories) || !Array.isArray(data.tasks)) {
      fail("ожидаются version: 1, массивы categories и tasks.");
    }
    const categories = new Set();
    for (const category of data.categories) {
      if (!category || typeof category.id !== "string" || !/^[a-z][a-z0-9-]*$/.test(category.id) ||
          category.id === "all" || categories.has(category.id) ||
          typeof category.title !== "string" || !category.title.trim()) fail("проверьте категории и уникальность их id.");
      categories.add(category.id);
    }
    const ids = new Set();
    const files = new Set();
    for (const task of data.tasks) {
      if (!task || !Number.isInteger(task.id) || task.id < 1 || ids.has(task.id)) fail("номер задачи должен быть уникальным положительным числом.");
      if (typeof task.title !== "string" || !task.title.trim()) fail(`у задачи ${task.id} нет названия.`);
      if (typeof task.file !== "string" || !/^\d+-[a-z0-9-]+\.md$/.test(task.file) ||
          Number(task.file.split("-")[0]) !== task.id || files.has(task.file)) fail(`проверьте Markdown-файл задачи ${task.id}.`);
      if (!has(priorities, task.priority) || !has(statuses, task.status)) fail(`проверьте priority и status задачи ${task.id}.`);
      if (!Array.isArray(task.categories) || !task.categories.length ||
          new Set(task.categories).size !== task.categories.length ||
          task.categories.some((id) => !categories.has(id))) fail(`проверьте categories задачи ${task.id}.`);
      ids.add(task.id);
      files.add(task.file);
    }
    return data;
  }

  function selectTasks(data, { category = "all", status = "all", search = "", sort = "priority-desc" } = {}) {
    const words = search.trim().toLocaleLowerCase("ru").split(/\s+/).filter(Boolean);
    const tasks = data.tasks.filter((task) =>
      (category === "all" || task.categories.includes(category)) &&
      (status === "all" || task.status === status) &&
      words.every((word) => `${String(task.id).padStart(2, "0")} ${task.title}`.toLocaleLowerCase("ru").includes(word)));
    return tasks.sort((a, b) => {
      if (sort === "title") return a.title.localeCompare(b.title, "ru") || a.id - b.id;
      if (sort === "id") return a.id - b.id;
      // Unassigned tasks always follow assigned tasks, in either direction.
      if ((a.priority === "none") !== (b.priority === "none")) return a.priority === "none" ? 1 : -1;
      const direction = sort === "priority-asc" ? -1 : 1;
      return direction * (priorities[a.priority].rank - priorities[b.priority].rank) || a.id - b.id;
    });
  }

  if (typeof module !== "undefined" && module.exports) module.exports = { BACKLOG_DATA, validateIndex, selectTasks, priorities };
  if (typeof document === "undefined") return;

  const $ = (id) => document.getElementById(id);
  let data;
  const state = { category: "all", status: "all", search: "", sort: "priority-desc" };

  function element(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function chooseCategory(id) {
    state.category = id;
    render();
  }

  function render() {
    const tasks = selectTasks(data, state);
    const categoryNames = new Map(data.categories.map((category) => [category.id, category.title]));
    const counts = new Map();
    for (const task of selectTasks(data, { ...state, category: "all" })) {
      for (const category of task.categories) counts.set(category, (counts.get(category) || 0) + 1);
    }
    // Keep category buttons mounted so keyboard focus survives filtering.
    for (const button of $("categories").children) {
      const id = button.dataset.category;
      button.setAttribute("aria-pressed", String(id === state.category));
      button.lastChild.textContent = id === "all"
        ? selectTasks(data, { ...state, category: "all" }).length : counts.get(id) || 0;
    }
    $("category-title").textContent = categoryNames.get(state.category) || "Все задачи";
    $("result-count").textContent = `${tasks.length} из ${data.tasks.length}`;
    $("empty").hidden = tasks.length > 0;
    const prioritySort = state.sort.startsWith("priority-");
    $("priority-heading").setAttribute("aria-sort", prioritySort ? (state.sort === "priority-desc" ? "descending" : "ascending") : "none");
    $("sort-arrow").textContent = prioritySort ? (state.sort === "priority-desc" ? "↓" : "↑") : "↕";
    const rows = document.createDocumentFragment();
    for (const task of tasks) {
      const row = element("tr");
      row.append(element("td", "number", String(task.id).padStart(2, "0")));
      const description = element("td");
      const link = element("a", "task-link", task.title);
      link.href = task.file;
      description.append(link);
      const tags = element("div", "tags");
      for (const category of task.categories) {
        const tag = element("button", "tag", categoryNames.get(category));
        tag.type = "button";
        tag.title = `Показать категорию «${categoryNames.get(category)}»`;
        tag.addEventListener("click", () => {
          chooseCategory(category);
          // The clicked row can disappear; move focus to the persistent filter.
          Array.from($("categories").children).find((button) => button.dataset.category === category).focus();
        });
        tags.append(tag);
      }
      description.append(tags);
      row.append(description);
      const status = element("td");
      status.append(element("span", `status${task.status === "in_progress" ? " active" : ""}`, statuses[task.status]));
      row.append(status);
      const priority = element("td");
      const badge = element("span", "priority", priorities[task.priority].label);
      badge.dataset.priority = task.priority;
      priority.append(badge);
      row.append(priority);
      rows.append(row);
    }
    $("task-list").replaceChildren(rows);
  }

  function loadIndex(value) {
    data = validateIndex(value);
    state.category = "all";
    $("total").textContent = data.tasks.length;
    $("in-progress").textContent = data.tasks.filter((task) => task.status === "in_progress").length;
    const categories = [{ id: "all", title: "Все задачи" }, ...data.categories];
    $("categories").replaceChildren(...categories.map((category) => {
      const button = element("button", "category");
      button.type = "button";
      button.dataset.category = category.id;
      button.append(element("span", "", category.title), element("span", "category-count"));
      button.addEventListener("click", () => chooseCategory(category.id));
      return button;
    }));
    $("load-message").hidden = true;
    $("workspace").hidden = false;
    render();
  }

  function showError(error) {
    $("workspace").hidden = true;
    $("total").textContent = $("in-progress").textContent = "—";
    $("load-message").textContent = `Не удалось загрузить индекс. ${error.message}`;
    $("load-message").hidden = false;
  }

  $("search").addEventListener("input", (event) => { state.search = event.target.value; render(); });
  $("status").addEventListener("change", (event) => { state.status = event.target.value; render(); });
  $("sort").addEventListener("change", (event) => { state.sort = event.target.value; render(); });
  $("priority-sort").addEventListener("click", () => {
    state.sort = state.sort === "priority-desc" ? "priority-asc" : "priority-desc";
    $("sort").value = state.sort;
    render();
  });
  $("reset").addEventListener("click", () => {
    Object.assign(state, { category: "all", status: "all", search: "", sort: "priority-desc" });
    $("search").value = "";
    $("status").value = "all";
    $("sort").value = state.sort;
    render();
  });
  try { loadIndex(BACKLOG_DATA); } catch (error) { showError(error); }
})();
