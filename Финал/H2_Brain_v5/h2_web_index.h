#pragma once
// =====================================================================================
//  H2 PRO — веб-интерфейс мозга (главная страница), вынесен из H2_Brain_v5.ino
//  Отдаётся по GET /. Встроенный JS работает через WebSocket /ws и HTTP-эндпоинты
//  мозга (см. раздел HTTP в H2_Brain_v5.ino).
//  ⚡ v6.0: выбор драйвера электролизёра убран (только MOSFET), выводной
//  вентилятор на реле (/setfans), новая вкладка «Звук» (/snd/*): мелодии событий,
//  библиотека мелодий и импорт MIDI / RTTTL / аудио с подготовкой под зуммер прямо
//  в браузере (без внешних библиотек — устройство работает в офлайн-сети).
//
//  ⚡ v6.3: ЭТОТ ФАЙЛ — ИСХОДНИК, в сборку прошивки он НЕ входит. Скетч подключает
//  h2_web_index_gz.h (gzip-образ страницы во флеше, отдаётся без копии в кучу).
//  ПОСЛЕ ПРАВКИ ЭТОГО ФАЙЛА ЗАПУСТИТЕ:  python tools/make_web_gz.py
//  (из папки H2_Brain_v5; проверка актуальности: python tools/make_web_gz.py --check)
// =====================================================================================
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="ru"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>H2 PRO v6.8</title>
<style>
  /* Системные шрифты: страница работает без доступа к интернету. */
  :root{
    --bg:#080810;--card:#131326;--border:#1e1e3a;
    --accent:#00d4ff;--ok:#00ff88;--warn:#ffcc00;--danger:#ff3355;--dim:#3a3a5a;--muted:#8a8fb0;--text:#c8d0e0;
    --purple:#bb88ff;--temp:#ff8844;
  }
  /* Светлая тема */
  body.light{
    --bg:#eef1f7;--card:#ffffff;--border:#d0d6e2;
    --accent:#0077cc;--ok:#007a3d;--warn:#9a6a00;--danger:#cc2244;--dim:#99a0b0;--muted:#5f6680;--text:#1a2030;
    --purple:#7744bb;--temp:#dd5500;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;
    min-height:100vh;padding:14px 10px;transition:background .3s,color .3s;
    background-image:radial-gradient(ellipse at 20% 0%,rgba(0,212,255,.07) 0%,transparent 60%),
      radial-gradient(ellipse at 80% 100%,rgba(77,138,255,.07) 0%,transparent 60%);}
  body.light{background-image:none;}
  .topbar{max-width:480px;margin:0 auto 10px;display:flex;justify-content:space-between;align-items:center;}
  h1{font-size:1.05em;font-weight:800;letter-spacing:5px;color:var(--accent);
    text-transform:uppercase;text-shadow:0 0 20px rgba(0,212,255,.5);}
  body.light h1{text-shadow:none;}
  .theme-btn{background:var(--card);border:1px solid var(--border);color:var(--text);
    border-radius:8px;padding:6px 10px;cursor:pointer;font-size:.9em;transition:all .2s;}
  .ws-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-left:6px;
    background:var(--dim);transition:all .3s;vertical-align:middle;}
  .ws-dot.live{background:var(--ok);box-shadow:0 0 8px var(--ok);}

  .panel{max-width:480px;margin:0 auto;display:flex;flex-direction:column;gap:10px;}

  /* Вкладки — ⚡ v6.0: шесть вкладок, сетка 3×2 (влезает на экран телефона) */
  .tabs{display:grid;grid-template-columns:repeat(3,1fr);gap:4px;background:var(--card);
    border:1px solid var(--border);border-radius:10px;padding:4px;}
  .tab{padding:8px 4px;border:none;background:transparent;color:var(--muted);
    border-radius:7px;cursor:pointer;font-family:'Exo 2',sans-serif;font-size:.7em;
    font-weight:600;letter-spacing:1px;text-transform:uppercase;transition:all .2s;}
  .tab.active{background:var(--accent);color:var(--bg);}
  .diag-row{display:flex;justify-content:space-between;align-items:center;gap:10px;
    padding:9px 2px;border-bottom:1px solid var(--border);font-size:.82em;}
  .diag-row:last-child{border-bottom:none;}
  .pill{font-size:.7em;font-weight:800;letter-spacing:1px;padding:3px 11px;border-radius:20px;
    border:1px solid var(--dim);color:var(--muted);white-space:nowrap;}
  .pill.ok{color:var(--ok);border-color:var(--ok);background:rgba(0,255,136,.08);}
  .pill.bad{color:var(--danger);border-color:var(--danger);background:rgba(255,51,85,.10);}
  .diag-hint{font-size:.8em;color:var(--text);line-height:1.6;}
  .page{display:none;flex-direction:column;gap:10px;}
  .page.active{display:flex;}

  .alert-bar{background:#1a1500;border:1px solid var(--warn);color:var(--warn);
    border-radius:10px;padding:8px 12px;text-align:center;font-size:.75em;
    letter-spacing:2px;text-transform:uppercase;font-family:'Share Tech Mono',monospace;display:none;}
  .alert-bar.show{display:block;animation:ap 1.5s infinite}
  .alert-bar.danger{background:#1a0008;border-color:var(--danger);color:var(--danger)}
  .alert-bar.tilt{background:#1a0a1a;border-color:#ff6bff;color:#ff6bff}
  @keyframes ap{0%,100%{opacity:.7}50%{opacity:1}}

  .pressure{background:var(--card);border:1px solid var(--border);border-radius:14px;
    padding:16px;text-align:center;position:relative;overflow:hidden;}
  .pressure::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
    background:linear-gradient(90deg,transparent,var(--accent),transparent);}
  .pressure.warn::before{background:linear-gradient(90deg,transparent,var(--warn),transparent)}
  .pressure.danger::before{background:linear-gradient(90deg,transparent,var(--danger),transparent)}
  .p-lbl{font-size:.7em;letter-spacing:4px;color:var(--muted);text-transform:uppercase;margin-bottom:6px}
  .p-num{font-family:'Share Tech Mono',monospace;font-size:3.4em;color:var(--accent);
    line-height:1;text-shadow:0 0 30px rgba(0,212,255,.6);transition:color .3s;}
  .p-num.warn{color:var(--warn)}.p-num.danger{color:var(--danger)}
  .p-unit{font-size:.4em;color:var(--muted);margin-left:6px}
  .p-bar-wrap{position:relative;margin-top:10px}
  .p-bar{height:6px;background:rgba(0,0,0,.3);border-radius:3px;overflow:hidden;position:relative;}
  body.light .p-bar{background:#e0e4ec;}
  /* ⚡ v6.2: журнал событий, строка таймера, ввод чисел */
  .log-list{max-height:360px;overflow-y:auto;}
  .log-row{display:grid;grid-template-columns:auto 1fr;gap:2px 10px;padding:8px 0;border-bottom:1px solid var(--border);font-size:.78em;}
  .log-row:last-child{border-bottom:none;}
  .log-t{font-family:'Share Tech Mono',monospace;color:var(--muted);font-size:.9em;white-space:nowrap;}
  .log-m{color:var(--text);}
  .log-row.bad .log-m{color:var(--danger);}
  .log-row.warn .log-m{color:var(--warn);}
  .inl{display:flex;gap:8px;align-items:center;}
  .inl .inp{flex:1;}
  .inl .data-btn{flex:none;padding:9px 12px;}
  .timer-ln{font-family:'Share Tech Mono',monospace;color:var(--accent);}

  @media (hover:hover){
    .theme-btn:hover{border-color:var(--accent);}
    .ctrl:hover:not(:disabled){filter:brightness(1.2)}
    .data-btn:hover{border-color:var(--accent);color:var(--accent);}
    .ota-link:hover{background:rgba(77,138,255,.1);border-color:#4d8aff;}
    .ico-btn:hover{border-color:var(--accent);color:var(--accent);}
    .ico-btn.del:hover{border-color:var(--danger);color:var(--danger);}
  }
  /* ⚡ v6.2: доступность и полировка */
  :focus-visible{outline:2px solid var(--accent);outline-offset:2px;}
  .inp:focus-visible{outline-offset:0;}
  /* Цифры одной ширины: показания не «прыгают» при обновлении */
  .p-num,.stat-v,.t-val,.tilt-val,.temp-val,#uptime,.timer-ln,.val,.log-t{font-variant-numeric:tabular-nums;}
  /* Пульсации и вращение — только если система не просит меньше движения;
     состояние при этом остаётся видно по цвету и тексту */
  @media (prefers-reduced-motion:reduce){
    *,*::before,*::after{animation:none!important;transition-duration:0s!important;}
  }
  .p-fill{height:100%;border-radius:3px;background:linear-gradient(90deg,var(--ok),var(--accent));
    transition:width .5s ease,background .3s;position:relative;z-index:1;}
  .p-fill.warn{background:linear-gradient(90deg,var(--accent),var(--warn))}
  .p-fill.danger{background:linear-gradient(90deg,var(--warn),var(--danger))}
  .p-tgt{position:absolute;top:-3px;width:2px;height:13px;background:var(--warn);
    border-radius:1px;transition:left .5s ease;pointer-events:none;z-index:2;}
  /* ⚡ v6.3: подписи шкалы по фактическому диапазону 0–6.5 бар (полоса и маркер — p/6.5) */
  .p-scale{position:relative;height:1.3em;font-size:.68em;color:var(--muted);
    font-family:'Share Tech Mono',monospace;margin-top:4px;letter-spacing:1px}
  .p-scale span{position:absolute;top:0;white-space:nowrap;}
  .p-scale .c{transform:translateX(-50%);}
  /* ⚡ v6.3: нет свежих данных от мозга — показания приглушены (тревоги — нет) */
  .stale .p-num,.stale .p-fill,.stale .badges,.stale .temp-val{opacity:.4}

  /* Температура + ⚡ v6.0: состояние двух вентиляторов (реле) */
  .temp-card{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:12px 16px;display:flex;justify-content:space-between;align-items:center;}
  .temp-left{display:flex;flex-direction:column;gap:2px;}
  .temp-lbl{font-size:.68em;letter-spacing:3px;color:var(--muted);text-transform:uppercase;}
  .temp-val{font-family:'Share Tech Mono',monospace;font-size:1.8em;color:var(--temp);line-height:1;}
  .temp-val.hot{color:var(--danger);animation:ap 1s infinite;}
  .fan-info{display:flex;flex-direction:column;gap:6px;font-size:.7em;
    font-family:'Share Tech Mono',monospace;color:var(--muted);}
  .fan-ln{display:flex;align-items:center;justify-content:flex-end;gap:7px;}
  .fan-ln b{font-weight:400;min-width:36px;text-align:right;}
  .fan-ln.on b{color:var(--accent);}
  .fan-spin{display:inline-block;}
  .fan-ln.on .fan-spin{color:var(--accent);animation:spin 1.6s linear infinite;}
  @keyframes spin{from{transform:rotate(0)}to{transform:rotate(360deg)}}

  .chart-box{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:10px 12px;}
  .chart-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;}
  .chart-lbl{font-size:.68em;letter-spacing:2px;color:var(--muted);text-transform:uppercase;}
  .range-btns{display:flex;gap:3px;}
  .range-btn{padding:3px 8px;border:1px solid var(--border);border-radius:6px;background:transparent;
    color:var(--muted);font-family:'Share Tech Mono',monospace;font-size:.68em;cursor:pointer;transition:all .2s;}
  .range-btn.active{border-color:var(--accent);color:var(--accent);background:rgba(0,212,255,.1);}
  canvas{width:100%;display:block;}
  #chartP{height:120px}#chartT{height:90px}   /* ⚡ v6.3: высота задана CSS — не зависит от размера буфера */

  .tilt-row{display:flex;justify-content:space-between;align-items:center;
    background:var(--card);border:1px solid var(--border);border-radius:10px;
    padding:8px 12px;font-size:.7em;letter-spacing:2px;
    font-family:'Share Tech Mono',monospace;color:var(--muted);}
  .tilt-row.warn{border-color:var(--warn);color:var(--warn);}
  .tilt-row.danger{border-color:#ff6bff;color:#ff6bff;animation:pulse .9s infinite;}
  .tilt-val{color:var(--ok);font-size:1.2em;}
  .tilt-val.warn{color:var(--warn);}.tilt-val.danger{color:#ff6bff;}

  .badges{display:flex;flex-wrap:wrap;gap:6px}
  .badge{padding:4px 10px;border-radius:20px;font-size:.68em;font-weight:600;
    letter-spacing:1.5px;text-transform:uppercase;background:var(--card);color:var(--muted);
    border:1px solid var(--border);transition:all .25s;}
  .badge.ok{background:#001a0e;color:var(--ok);border-color:var(--ok);}
  body.light .badge.ok{background:#e0fff0;}
  .badge.warn{background:#1a1500;color:var(--warn);border-color:var(--warn);}
  body.light .badge.warn{background:#fff8e0;}
  .badge.danger{background:#1a0008;color:var(--danger);border-color:var(--danger);animation:pulse .9s infinite}
  .badge.tilt{background:#1a0a1a;color:#ff6bff;border-color:#ff6bff;animation:pulse .9s infinite}
  .badge.purple{background:#1a0a2a;color:var(--purple);border-color:var(--purple);animation:pulse .9s infinite}
  @keyframes pulse{0%,100%{box-shadow:0 0 6px rgba(255,51,85,.3)}50%{box-shadow:0 0 14px rgba(255,51,85,.7)}}

  .info-row{display:flex;justify-content:space-between;align-items:center;padding:0 4px;
    font-size:.68em;letter-spacing:2px;color:var(--muted);font-family:'Share Tech Mono',monospace;}
  .rf-indicator{display:flex;align-items:center;gap:6px;}
  .rf-dot{width:7px;height:7px;border-radius:50%;background:var(--dim);transition:all .3s}
  .rf-dot.ok{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .rf-dot.fail{background:var(--danger);animation:pulse .9s infinite}

  .btn-power{width:100%;padding:16px;border:none;border-radius:12px;cursor:pointer;
    font-family:'Exo 2',sans-serif;font-size:.9em;font-weight:800;letter-spacing:3px;
    text-transform:uppercase;transition:all .2s;
    background:var(--card);color:var(--text);border:1px solid var(--border);}
  .btn-power.on{background:linear-gradient(135deg,#1a0010,#330015);color:var(--danger);
    border-color:var(--danger);box-shadow:0 0 20px rgba(255,51,85,.2);}
  body.light .btn-power.on{background:#ffe0e6;}
  .btn-power:active{transform:scale(.97)}

  .grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .ctrl{padding:14px 8px;border:1px solid var(--border);border-radius:11px;
    background:var(--card);cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.7em;font-weight:600;letter-spacing:1px;text-transform:uppercase;
    color:var(--text);transition:all .2s;display:flex;flex-direction:column;
    align-items:center;gap:8px;}
  .ctrl .dot{width:8px;height:8px;border-radius:50%;background:var(--dim);transition:all .25s}
  .ctrl.wide{grid-column:1/-1}
  .ctrl:active:not(:disabled){transform:scale(.95)}
  .ctrl:disabled{opacity:.25;cursor:not-allowed}
  .ctrl.active{background:#001a0e;border-color:var(--ok)}
  .ctrl.active .dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .ctrl.active-warn{background:#1a1500;border-color:var(--warn)}
  .ctrl.active-warn .dot{background:var(--warn);box-shadow:0 0 8px var(--warn)}
  .ctrl.active-red{background:#1a0008;border-color:var(--danger)}
  .ctrl.active-red .dot{background:var(--danger);box-shadow:0 0 8px var(--danger)}
  .ctrl.active-purple{background:#1a0a2a;border-color:var(--purple)}
  .ctrl.active-purple .dot{background:var(--purple);box-shadow:0 0 8px var(--purple)}
  .ctrl.purge-btn{background:#0a1a0a;border-color:#2a4a2a;color:#8f8;}
  .ctrl.repulsor-btn{background:#150a2a;border-color:#3a2a5a;color:var(--purple);}

  .footer{text-align:center;font-size:.7em;letter-spacing:2px;color:var(--warn);
    min-height:16px;font-family:'Share Tech Mono',monospace;}

  .block{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:14px;position:relative;overflow:hidden;}
  .block::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;}
  .block.green::before{background:linear-gradient(90deg,transparent,#56d364,transparent);}
  .block.warn::before{background:linear-gradient(90deg,transparent,var(--warn),transparent);}
  .block.temp::before{background:linear-gradient(90deg,transparent,var(--temp),transparent);}
  .blk-lbl{font-size:.68em;letter-spacing:4px;color:var(--muted);text-transform:uppercase;
    text-align:center;margin-bottom:10px;}
  /* ⚡ v6.0: заголовок карточки с состоянием справа */
  .blk-head{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:12px;}
  .blk-head .blk-lbl{margin:0;text-align:left;}
  .blk-aux{font-family:'Share Tech Mono',monospace;font-size:.68em;letter-spacing:1px;color:var(--muted);white-space:nowrap;}

  .seg{display:flex;gap:6px;margin-bottom:10px;}
  .seg-opt{flex:1;padding:9px 4px;border:1px solid var(--border);border-radius:8px;
    background:transparent;cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.7em;font-weight:600;letter-spacing:1px;text-transform:uppercase;
    color:var(--muted);text-align:center;transition:all .2s;}
  .seg-opt.active{border-color:var(--accent);color:var(--accent);background:rgba(0,212,255,.1);}

  .row{display:flex;align-items:center;gap:8px;margin-bottom:8px;}
  .row label{font-size:.68em;letter-spacing:2px;color:var(--muted);text-transform:uppercase;min-width:62px;}
  .row.lw label{min-width:104px;}
  .row .val{font-family:'Share Tech Mono',monospace;font-size:.95em;color:var(--accent);min-width:70px;text-align:right;}
  input[type=range]{flex:1;-webkit-appearance:none;height:4px;border-radius:2px;cursor:pointer;
    background:linear-gradient(90deg,var(--accent) var(--pct,0%),var(--border) var(--pct,0%));}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;
    border-radius:50%;background:var(--accent);cursor:pointer;}
  input[type=range]:disabled{opacity:.4;}

  .t-pm{width:38px;height:38px;border-radius:9px;border:1px solid var(--warn);
    background:rgba(255,204,0,.1);color:var(--warn);font-size:1.3em;font-weight:800;
    cursor:pointer;display:flex;align-items:center;justify-content:center;transition:all .2s;}
  .t-pm:active{transform:scale(.92);}
  .t-val{font-family:'Share Tech Mono',monospace;font-size:1.9em;color:var(--warn);min-width:88px;text-align:center;}
  .t-unit{font-size:.42em;color:var(--muted);}
  .qrow{display:flex;gap:5px;margin-bottom:10px;}
  .qb{flex:1;padding:6px 2px;border:1px solid var(--border);border-radius:7px;background:transparent;
    cursor:pointer;font-family:'Share Tech Mono',monospace;font-size:.7em;color:var(--muted);transition:all .2s;text-align:center;}
  .qb.active{border-color:var(--warn);color:var(--warn);background:rgba(255,204,0,.1);}
  .apply-btn{width:100%;padding:10px;border:1px solid var(--accent);border-radius:9px;
    background:rgba(0,212,255,.1);color:var(--accent);cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.72em;font-weight:800;letter-spacing:2px;text-transform:uppercase;transition:all .2s;}
  .apply-btn:active{transform:scale(.97);}
  .ok-msg{text-align:center;font-size:.68em;font-family:'Share Tech Mono',monospace;
    color:var(--ok);min-height:14px;margin-top:5px;transition:opacity .4s;opacity:0;}

  /* Статистика */
  .stat-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
  .stat-item{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:10px 12px;
    display:flex;flex-direction:column;justify-content:space-between;}
  .stat-k{font-size:.68em;letter-spacing:2px;color:var(--muted);text-transform:uppercase;margin-bottom:3px;}
  .stat-v{font-family:'Share Tech Mono',monospace;font-size:1.2em;color:var(--accent);}
  .stat-v.sm{font-size:.95em;}

  .data-btns{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
  .data-btn{padding:11px;border:1px solid var(--border);border-radius:9px;background:var(--card);
    color:var(--text);cursor:pointer;font-family:'Exo 2',sans-serif;font-size:.7em;font-weight:600;
    letter-spacing:1px;text-transform:uppercase;transition:all .2s;}
  .data-btn:active{transform:scale(.96);}
  .data-note{font-size:.68em;color:var(--muted);text-align:center;line-height:1.6;
    font-family:'Share Tech Mono',monospace;margin-top:4px;}

  .ota-link{display:block;text-align:center;font-size:.68em;letter-spacing:2px;
    color:#4d8aff;text-decoration:none;padding:8px;border:1px dashed #1e3a5a;
    border-radius:8px;transition:all .2s;font-family:'Share Tech Mono',monospace;text-transform:uppercase;}
  .ota-link.disabled{color:var(--muted);border-color:var(--border);cursor:not-allowed;}

  /* ⚡ v6.0: общие элементы форм (вентиляторы, звук) */
  .apply-btn:disabled,.data-btn:disabled,.ico-btn:disabled{opacity:.35;cursor:not-allowed;transform:none;}
  .sub-lbl{font-size:.68em;letter-spacing:2px;color:var(--muted);text-transform:uppercase;margin-bottom:6px;}
  .hint{font-size:.72em;color:var(--muted);line-height:1.5;margin-bottom:10px;}
  .fld{margin-bottom:12px;}
  .inp{width:100%;padding:9px 10px;border:1px solid var(--border);border-radius:8px;background:transparent;
    color:var(--text);font-family:'Exo 2',sans-serif;font-size:.85em;transition:border-color .2s;}
  .inp:focus{border-color:var(--accent);}
  select.inp{-webkit-appearance:none;appearance:none;padding-right:30px;cursor:pointer;
    background-image:linear-gradient(45deg,transparent 50%,var(--dim) 50%),linear-gradient(135deg,var(--dim) 50%,transparent 50%);
    background-position:calc(100% - 16px) 50%,calc(100% - 11px) 50%;background-size:5px 5px;background-repeat:no-repeat;}
  select.inp option{background:var(--card);color:var(--text);}
  textarea.inp{font-family:'Share Tech Mono',monospace;font-size:.75em;resize:vertical;min-height:62px;line-height:1.4;}
  .row select.inp{flex:1;width:auto;}
  @supports (-webkit-touch-callout:none){ .inp,textarea.inp{font-size:16px;} }  /* iOS: без автозума */
  .sw{-webkit-appearance:none;appearance:none;flex:none;width:36px;height:20px;border-radius:10px;
    border:1px solid var(--border);background:transparent;position:relative;cursor:pointer;transition:all .2s;}
  .sw::after{content:'';position:absolute;top:3px;left:3px;width:12px;height:12px;border-radius:50%;
    background:var(--dim);transition:all .2s;}
  .sw:checked{border-color:var(--accent);background:rgba(0,212,255,.12);}
  .sw:checked::after{left:19px;background:var(--accent);}
  .opt-list{border-top:1px solid var(--border);margin-bottom:12px;}
  .opt-list .diag-row{cursor:pointer;}
  .btn-row{display:grid;grid-template-columns:2fr 1fr 1fr;gap:8px;}
  .btn-row.two{grid-template-columns:1fr 1fr;}
  .btn-row>button{padding:10px 6px;}

  /* ⚡ v6.0: сворачиваемые секции */
  .fold>summary{list-style:none;cursor:pointer;display:flex;justify-content:space-between;align-items:center;
    -webkit-tap-highlight-color:transparent;}
  .fold>summary::-webkit-details-marker{display:none;}
  .fold>summary::after{content:'';width:7px;height:7px;border-right:1.5px solid var(--dim);
    border-bottom:1.5px solid var(--dim);transform:rotate(45deg);transition:transform .2s;margin:0 4px 4px 0;}
  .fold[open]>summary::after{transform:rotate(-135deg);margin:4px 4px 0 0;}
  .fold>summary .blk-lbl{margin:0;text-align:left;}
  .fold:not(.sub)>summary .blk-lbl{color:var(--accent);}
  .fold:not(.sub)>summary::after{border-color:var(--accent);}
  .fold[open]>summary{margin-bottom:14px;}
  .fold.sub{border-top:1px solid var(--border);padding-top:11px;margin:2px 0 12px;}
  .fold.sub>summary{font-size:.68em;letter-spacing:2px;color:var(--muted);text-transform:uppercase;}
  .fold.sub[open]>summary{margin-bottom:12px;}

  /* ⚡ v6.0: библиотека мелодий */
  .mel-row{display:flex;align-items:center;gap:6px;padding:9px 0;border-bottom:1px solid var(--border);}
  .mel-row:last-child{border-bottom:none;}
  .mel-main{flex:1;min-width:0;}
  .mel-name{font-size:.82em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .mel-meta{font-size:.68em;color:var(--muted);font-family:'Share Tech Mono',monospace;margin-top:2px;}
  .mel-empty{font-size:.75em;color:var(--muted);text-align:center;padding:10px 0 12px;
    border-bottom:1px solid var(--border);}
  .ico-btn{flex:none;width:34px;height:34px;display:inline-flex;align-items:center;justify-content:center;
    border:1px solid var(--border);border-radius:8px;background:transparent;color:var(--muted);
    cursor:pointer;transition:all .2s;}
  .ico-btn svg{width:15px;height:15px;fill:none;stroke:currentColor;stroke-width:2;
    stroke-linecap:round;stroke-linejoin:round;}
  .ico-btn.on{border-color:var(--accent);color:var(--accent);}
  .ico-btn:active{transform:scale(.94);}
  .ico-ph{flex:none;width:34px;}

  /* ⚡ v6.0: импорт мелодии */
  .file-pick{display:flex;align-items:center;gap:10px;margin-bottom:12px;}
  .file-pick .data-btn{flex:none;}
  .file-name{font-size:.75em;color:var(--muted);min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
  .prog{height:3px;background:var(--border);border-radius:2px;overflow:hidden;margin-top:10px;display:none;}
  .prog.show{display:block;}
  .prog>i{display:block;height:100%;width:0;background:var(--accent);transition:width .15s;}
  .msg{font-size:.72em;min-height:16px;margin-top:8px;color:var(--muted);line-height:1.5;}
  .msg.ok{color:var(--ok);}
  .msg.bad{color:var(--danger);}
  .mel-res{display:none;margin-top:12px;padding-top:12px;border-top:1px solid var(--border);}
  .mel-res.show{display:block;}
  .mel-stat{font-family:'Share Tech Mono',monospace;font-size:.85em;color:var(--accent);margin-bottom:4px;}
  .mel-tips{margin-top:14px;padding-top:12px;border-top:1px solid var(--border);}
  .mel-tips p{font-size:.7em;color:var(--muted);line-height:1.55;}
  .mel-tips p+p{margin-top:8px;}
</style></head>
<body>
<div class="topbar">
  <h1>⚡ H2 PRO<span class="ws-dot" id="wsDot"></span></h1>
  <button class="theme-btn" id="themeBtn" onclick="toggleTheme()" aria-label="Сменить тему">🌙</button>
</div>

<div class="panel">
  <div class="tabs">
    <button class="tab active" onclick="showPage('main',this)">Управление</button>
    <button class="tab" onclick="showPage('graph',this)">Графики</button>
    <button class="tab" onclick="showPage('settings',this)">Настройки</button>
    <button class="tab" onclick="showPage('stats',this)">Данные</button>
    <button class="tab" onclick="showPage('sound',this)">Звук</button>
    <button class="tab" onclick="showPage('diag',this)">Диагн.</button>
  </div>
  <!-- ⚡ v6.3: нет свежих данных (WS и HTTP молчат) — видно на любой вкладке -->
  <div class="alert-bar danger" id="linkBar"></div>

  <!-- ════════ СТРАНИЦА: УПРАВЛЕНИЕ ════════ -->
  <div class="page active" id="page-main">
    <div class="alert-bar" id="alertBar" role="status" aria-live="polite"></div>

    <div class="pressure" id="pBox">
      <div class="p-lbl">Давление в баке</div>
      <div class="p-num" id="pNum"><span id="pv">0.00</span><span class="p-unit">бар</span></div>
      <div class="p-bar-wrap">
        <div class="p-bar">
          <div class="p-fill" id="pf" style="width:0%"></div>
        </div>
        <div class="p-tgt" id="ptgt" style="left:15.4%"></div>
        <div class="p-scale"><span style="left:0">0</span><span class="c" style="left:30.77%">2</span><span class="c" style="left:61.54%">4</span><span style="right:0">6.5 бар</span></div>
      </div>
    </div>

    <div class="temp-card block temp">
      <div class="temp-left">
        <div class="temp-lbl">Температура</div>
        <div class="temp-val" id="tempVal">--°</div>
      </div>
      <div class="fan-info">
        <div class="fan-ln" id="fanL1"><span class="fan-spin">❄</span>корпус<b>питание напрямую</b></div>
        <div class="fan-ln" id="fanL2"><span class="fan-spin">❄</span>выводной<b id="fanTx2">—</b></div>
      </div>
    </div>

    <div class="tilt-row" id="tiltRow">
      <span id="tiltLbl">НАКЛОН (порог 30°)</span>
      <span class="tilt-val" id="tiltVal">--°</span>
    </div>

    <div class="badges">
      <span class="badge" id="bPwr">POWER</span>
      <span class="badge" id="bLck">LOCK</span>
      <span class="badge" id="bTlt">TILT</span>
      <span class="badge" id="bPrg">PURGE</span>
      <span class="badge" id="bPsh">PURGE SHORT</span>
      <span class="badge" id="bRep">РЕПУЛЬСОР</span>
      <span class="badge" id="bMgs">MANUAL</span>
      <span class="badge" id="bPzo">PIEZO</span>
      <span class="badge" id="bEle">ELECTRO</span>
      <span class="badge" id="bVal">VALVE</span>
    </div>

    <div class="info-row">
      <div class="rf-indicator"><div class="rf-dot" id="rfDot"></div><span id="rfTxt">ПУЛЬТ: —</span></div>
      <div><span class="timer-ln" id="tmrLn"></span> <span id="uptime">UP 00:00:00</span></div>
    </div>

    <button class="btn-power" id="btnPwr" onclick="handlePower()">ВКЛЮЧИТЬ СИСТЕМУ</button>
    <button class="apply-btn" id="btnMute" style="display:none;margin-bottom:12px;" onclick="cmd(12)">🔕 ЗАГЛУШИТЬ ТРЕВОГУ</button>

    <div class="grid">
      <button class="ctrl wide" id="cLck" onclick="cmd(2)"><div class="dot"></div>БЛОКИРОВКА / СБРОС АВАРИЙ</button>
      <button class="ctrl repulsor-btn" id="cRep" onclick="cmd(3)" disabled><div class="dot"></div>РЕПУЛЬСОР (800 мс)</button>
      <button class="ctrl" id="cMgs" onclick="cmd(7)" disabled><div class="dot"></div>РУЧНАЯ ПОДАЧА</button>
      <button class="ctrl active-warn wide" id="cPzo" onclick="cmd(4)" disabled><div class="dot"></div>ПОДЖИГ (500 мс)</button>
      <button class="ctrl purge-btn wide" id="cPsh" onclick="cmd(5)" disabled><div class="dot"></div>ПРОДУТЬ (200 мс клапан)</button>
    </div>
    <div class="ok-msg" id="cmdMsg"></div>

    <div class="block warn">
      <div class="blk-lbl">Целевое давление (max 6.5 бар)</div>
      <div class="row" style="justify-content:center;">
        <button class="t-pm" onclick="stepTgt(-0.5)">−</button>
        <span class="t-val"><span id="tval">1.0</span><span class="t-unit"> бар</span></span>
        <button class="t-pm" onclick="stepTgt(+0.5)">+</button>
      </div>
      <div class="row"><input type="range" id="tslider" min="0.5" max="6.0" step="0.1" value="1.0" oninput="onSlide(this.value)"></div>
      <div class="qrow">
        <button class="qb active" onclick="setQ(1)">1</button><button class="qb" onclick="setQ(2)">2</button>
        <button class="qb" onclick="setQ(3)">3</button><button class="qb" onclick="setQ(4)">4</button>
        <button class="qb" onclick="setQ(5)">5</button><button class="qb" onclick="setQ(6)">6</button>
      </div>
      <button class="apply-btn" onclick="applyTgt()">УСТАНОВИТЬ ДАВЛЕНИЕ</button>
      <div class="ok-msg" id="tok"></div>
    </div>

    <div class="footer" id="foot"></div>
  </div>

  <!-- ════════ СТРАНИЦА: ГРАФИКИ ════════ -->
  <div class="page" id="page-graph">
    <div class="chart-box">
      <div class="chart-hdr">
        <span class="chart-lbl">Давление, бар</span>
        <div class="range-btns">
          <button class="range-btn active" onclick="setRange('1h',this)">1ч</button>
          <button class="range-btn" onclick="setRange('1d',this)">1д</button>
          <button class="range-btn" onclick="setRange('1w',this)">1н</button>
        </div>
      </div>
      <canvas id="chartP" height="120"></canvas>
    </div>
    <div class="chart-box">
      <div class="chart-hdr"><span class="chart-lbl">Температура, °C</span></div>
      <canvas id="chartT" height="90"></canvas>
    </div>
    <div class="data-note" id="histInfo">Точек в базе: 0</div>
  </div>

  <!-- ════════ СТРАНИЦА: НАСТРОЙКИ ════════ -->
  <div class="page" id="page-settings">
    <!-- ⚡ v6.2: таймер работы (гистерезис убран: MOSFET вкл при P < цели) -->
    <div class="block green">
      <div class="blk-head"><span class="blk-lbl">Таймер работы</span><span class="pill" id="tmrSt">—</span></div>
      <div class="hint">Через сколько выключить систему после включения. Электролизёр работает по цели давления, без гистерезиса.</div>
      <div class="inl">
        <select class="inp" id="tmrSel" onchange="tmrDirty=true;tmrPaint()">
          <option value="0">Без ограничения</option>
          <option value="15">15 мин</option><option value="30">30 мин</option>
          <option value="60">1 час</option><option value="120">2 часа</option>
          <option value="240">4 часа</option><option value="480">8 часов</option>
          <option value="-1">Своё значение…</option>
        </select>
        <input class="inp" type="number" id="tmrMin" min="1" max="600" step="1" placeholder="мин" style="display:none;max-width:90px" oninput="tmrDirty=true">
      </div>
      <button class="apply-btn" style="margin-top:10px" onclick="tmrApply()">Применить</button>
      <div class="ok-msg" id="tmrOk"></div>
    </div>

    <!-- ⚡ v6.2: обслуживание -->
    <div class="block warn">
      <div class="blk-head"><span class="blk-lbl">Обслуживание</span><span class="pill" id="mntSt">—</span></div>
      <div class="hint">Напоминание долить воду/электролит через заданное число часов работы электролизёра.</div>
      <div class="inl">
        <input class="inp" type="number" id="mntI" min="1" max="1000" step="1" oninput="mntDirty=true">
        <span class="hint" style="margin:0">ч наработки</span>
      </div>
      <div class="btn-row two" style="margin-top:10px">
        <button class="apply-btn" onclick="mntApply()">Сохранить</button>
        <button class="data-btn" onclick="mntDone()">✓ Обслужено</button>
      </div>
      <div class="ok-msg" id="mntOk"></div>
    </div>

    <div class="block temp"><div class="blk-lbl">Вентилятор корпуса</div><div class="data-note">Постоянное питание, без реле и управления ESP32.</div></div>
    <div class="block temp" id="fanCard2"></div>

    <!-- Калибровка давления -->
    <div class="block green">
      <div class="blk-lbl">Калибровка датчика давления</div>
      <div class="data-note" style="margin-bottom:10px;">2 точки: при 0 бар запишите текущее напряжение (offset), затем масштаб (бар/В). Только при выключенной системе и стравленном баке. Текущее давление: <b id="calCur">0.00</b> бар · напряжение сейчас: <b id="calPv">—</b> В</div>
      <div class="row">
        <label>Offset В</label>
        <input type="range" id="calOffSlider" min="0" max="2" step="0.01" value="0.5" oninput="onCalOff(this.value)">
        <span class="val" id="calOffVal">0.500 В</span>
      </div>
      <div class="row">
        <label>Бар/В</label>
        <input type="range" id="calSclSlider" min="0.1" max="10" step="0.05" value="2.5" oninput="onCalScl(this.value)">
        <span class="val" id="calSclVal">2.50</span>
      </div>
      <button class="data-btn" style="width:100%;margin-bottom:8px" onclick="calZero()">Взять текущее напряжение как 0 бар</button>
      <button class="apply-btn" onclick="applyCal()">СОХРАНИТЬ КАЛИБРОВКУ</button>
      <div class="ok-msg" id="calOk"></div>
    </div>

    <a class="ota-link" id="otaLink" href="/update">⬆ OTA-обновление прошивки</a>
  </div>

  <!-- ════════ СТРАНИЦА: ДАННЫЕ/СТАТИСТИКА ════════ -->
  <div class="page" id="page-stats">
    <div class="stat-grid">
      <div class="stat-item"><div class="stat-k">Давление сейчас</div><div class="stat-v" id="sPnow">0.00</div></div>
      <div class="stat-item"><div class="stat-k">Давление макс</div><div class="stat-v" id="sPmax">0.00</div></div>
      <div class="stat-item"><div class="stat-k">Темп. сейчас</div><div class="stat-v" id="sTnow">0.0°</div></div>
      <div class="stat-item"><div class="stat-k">Темп. макс</div><div class="stat-v" id="sTmax">0.0°</div></div>
      <div class="stat-item"><div class="stat-k">Наработка</div><div class="stat-v sm" id="sRt">0ч 0м</div></div>
      <div class="stat-item"><div class="stat-k">Аптайм</div><div class="stat-v sm" id="sUp">0:00:00</div></div>
    </div>

    <!-- ⚡ v6.2: счётчик газа (оценка по росту давления при закрытом клапане) -->
    <div class="block green">
      <div class="blk-lbl">Водород (оценка)</div>
      <div class="stat-grid">
        <div class="stat-item"><div class="stat-k">Сейчас, нл/ч</div><div class="stat-v" id="gRate">0.0</div></div>
        <div class="stat-item"><div class="stat-k">Загрузка MOSFET</div><div class="stat-v" id="gDuty">0%</div></div>
        <div class="stat-item"><div class="stat-k">За сессию</div><div class="stat-v sm" id="gSes">0 нл</div></div>
        <div class="stat-item"><div class="stat-k">Сегодня</div><div class="stat-v sm" id="gDay">—</div></div>
        <div class="stat-item"><div class="stat-k">7 дней</div><div class="stat-v sm" id="gWk">—</div></div>
        <div class="stat-item"><div class="stat-k">Всего</div><div class="stat-v sm" id="gTot">0 нл</div></div>
      </div>
      <div class="sub-lbl" style="margin-top:12px">Объём бака, л</div>
      <div class="inl">
        <input class="inp" type="number" id="gTank" min="0.1" max="100" step="0.1" oninput="gasDirty=true">
        <button class="data-btn" onclick="gasApply()">Сохранить</button>
      </div>
      <div class="hint" style="margin-top:8px">Считается по приросту давления, пока электролизёр работает при закрытом клапане: ΔP × V / 1,013. Расходомера нет — это оценка. «Сегодня» и «7 дней» видны после синхронизации часов с телефоном.</div>
      <button class="data-btn" style="width:100%;border-color:var(--danger);color:var(--danger)" onclick="gasReset()">Обнулить счётчик газа</button>
      <div class="ok-msg" id="gOk"></div>
    </div>

    <div class="block warn">
      <div class="blk-lbl">Циклы реле <span style="font-size:.8em;color:var(--muted)">(ресурс ~100k)</span></div>
      <div class="stat-grid">
        <div class="stat-item"><div class="stat-k">Клапан</div><div class="stat-v sm" id="sRcv">0</div></div>
        <div class="stat-item"><div class="stat-k">Пьезо</div><div class="stat-v sm" id="sRcp">0</div></div>
        <div class="stat-item"><div class="stat-k">Выводной вентилятор</div><div class="stat-v sm" id="sRcf2">0</div></div>
        <div class="stat-item"><div class="stat-k">Электролизёр (MOSFET), переключений</div><div class="stat-v sm" id="sRce">0</div></div>
        <div class="stat-item"><div class="stat-k">RF-каналов</div><div class="stat-v sm" id="sRfh">к76 / 0</div></div>
      </div>
      <button class="apply-btn" style="margin-top:10px;border-color:var(--danger);color:var(--danger);background:rgba(255,51,85,.1);" onclick="resetCycles()">СБРОСИТЬ ЦИКЛЫ РЕЛЕ</button>
      <div class="ok-msg" id="cycOk"></div>
    </div>

    <div class="block green">
      <div class="blk-lbl">База данных истории (на телефоне)</div>
      <div class="data-btns">
        <button class="data-btn" onclick="exportCSV()">⬇ Экспорт CSV</button>
        <button class="data-btn" onclick="exportJSON()">⬇ Экспорт JSON</button>
        <button class="data-btn" onclick="document.getElementById('importFile').click()">⬆ Импорт файла</button>
        <button class="data-btn" style="border-color:var(--danger);color:var(--danger);" onclick="clearHistory()">🗑 Очистить базу</button>
      </div>
      <input type="file" id="importFile" accept=".json,.csv" style="display:none" onchange="importData(this)">
      <div class="data-note" id="dbInfo">Точек: 0 · Размер: 0 КБ · Авто-сохранение: вкл</div>
    </div>
    <button class="apply-btn" onclick="resetRT()" style="border-color:var(--danger);color:var(--danger);background:rgba(255,51,85,.1);">СБРОСИТЬ СЧЁТЧИК НАРАБОТКИ</button>
    <div class="ok-msg" id="rtOk"></div>
  </div>

  <!-- ════════ ⚡ v6.0 СТРАНИЦА: ЗВУК ════════ -->
  <div class="page" id="page-sound">
    <div class="block">
      <div class="blk-head"><span class="blk-lbl">Звук</span><span class="pill ok" id="sndPlay" style="visibility:hidden">ИГРАЕТ</span></div>
      <div class="sub-lbl">Где играть</div>
      <div class="seg" id="sndOutSeg">
        <button class="seg-opt" data-v="0" onclick="sndSetOut(0)">Пульт</button>
        <button class="seg-opt" data-v="1" onclick="sndSetOut(1)">Мозг</button>
        <button class="seg-opt" data-v="2" onclick="sndSetOut(2)">Оба</button>
        <button class="seg-opt" data-v="3" onclick="sndSetOut(3)">Выкл</button>
      </div>
      <div class="row">
        <label>Громкость</label>
        <input type="range" id="sndVol" min="1" max="10" step="1" value="10" oninput="sndOnVol(this.value)">
        <span class="val" id="sndVolVal">10</span>
      </div>
      <div class="opt-list">
        <label class="diag-row">Звуки тревог<input type="checkbox" class="sw" id="sndAlm" checked onchange="sndDirty=true"></label>
        <label class="diag-row">Щелчки кнопок<input type="checkbox" class="sw" id="sndClk" checked onchange="sndDirty=true"></label>
      </div>
      <div class="fld">
        <div class="sub-lbl">Мелодия при загрузке</div>
        <select class="inp" id="sndBoot" onchange="sndDirty=true"></select>
      </div>
      <div class="fld">
        <div class="sub-lbl">Мелодия при включении системы</div>
        <select class="inp" id="sndOn" onchange="sndDirty=true"></select>
      </div>
      <div class="fld">
        <div class="sub-lbl">Мелодия при выключении</div>
        <select class="inp" id="sndOff" onchange="sndDirty=true"></select>
      </div>
      <div class="fld">
        <div class="sub-lbl">Продувка завершена</div>
        <select class="inp" id="sndPur" onchange="sndDirty=true"></select>
      </div>
      <div class="fld">
        <div class="sub-lbl">Тревога (своя — не длиннее 10 с)</div>
        <select class="inp" id="sndAlr" onchange="sndDirty=true"></select>
      </div>
      <div class="fld">
        <div class="sub-lbl">Предупреждение (своё — не длиннее 10 с)</div>
        <select class="inp" id="sndWrn" onchange="sndDirty=true"></select>
      </div>
      <div class="btn-row">
        <button class="apply-btn" onclick="sndApply()">Применить</button>
        <button class="data-btn" onclick="sndTest()">Тест</button>
        <button class="data-btn" onclick="sndStop()">■ Стоп</button>
      </div>
      <div class="ok-msg" id="sndOk"></div>
    </div>

    <div class="block">
      <div class="blk-head"><span class="blk-lbl">Библиотека мелодий</span><span class="blk-aux" id="melSlots"></span></div>
      <div id="melList"><div class="mel-empty">Загрузка…</div></div>
      <div class="data-note" id="melInfo"></div>
      <div class="ok-msg" id="melOk"></div>
    </div>

    <div class="block">
      <div class="blk-head"><span class="blk-lbl">Мелодии пульта</span><span class="pill" id="rmSt">…</span></div>
      <div id="rmList"><div class="mel-empty">Загрузка…</div></div>
      <div class="fld">
        <div class="sub-lbl">Кнопка МЕЛОДИЯ на пульте играет</div>
        <select class="inp" id="rmAct" onchange="rmDirty=true"></select>
      </div>
      <div class="fld">
        <div class="sub-lbl">Звук при включении пульта</div>
        <select class="inp" id="rmBoot" onchange="rmDirty=true"></select>
      </div>
      <div class="btn-row">
        <button class="apply-btn" onclick="rmApply()">Применить на пульте</button>
        <button class="data-btn" onclick="rmLoad(true)">Обновить</button>
      </div>
      <div class="prog" id="rmProg"><i id="rmProgBar"></i></div>
      <div class="ok-msg" id="rmOk"></div>
      <div class="data-note">До 8 своих мелодий хранятся в памяти пульта и передаются по радио через мозг — пульт должен быть включён и на связи. Мелодию из библиотеки мозга можно отправить в пульт кнопкой со стрелкой.</div>
    </div>

    <details class="block fold" id="melAdd">
      <summary><span class="blk-lbl">Добавить мелодию</span></summary>
      <div class="file-pick">
        <button class="data-btn" onclick="document.getElementById('melFile').click()">Выбрать файл</button>
        <span class="file-name" id="melFileName">MIDI, RTTTL или аудио (mp3, wav, ogg, m4a)</span>
      </div>
      <input type="file" id="melFile" style="display:none" onchange="melOnFile(this)">
      <div class="fld">
        <div class="sub-lbl">или вставьте RTTTL-текст</div>
        <textarea class="inp" id="melText" rows="3" spellcheck="false" placeholder="Name:d=4,o=5,b=120:8c,8e,8g,c6" oninput="melOnText()"></textarea>
      </div>
      <div class="fld" id="melTrkFld" style="display:none">
        <div class="sub-lbl">Дорожка MIDI</div>
        <select class="inp" id="melTrk" onchange="melOnTrack()"></select>
      </div>
      <div class="sub-lbl" id="melSegLbl" style="display:none">Отрезки трека</div>
      <div id="melSeg"></div>
      <details class="fold sub">
        <summary>Параметры подготовки</summary>
        <div class="row lw"><label>Октавы</label>
          <select class="inp" id="melOct" onchange="melOnParam()">
            <option value="auto">Авто — под зуммер</option><option value="-3">−3</option><option value="-2">−2</option>
            <option value="-1">−1</option><option value="0">0</option><option value="1">+1</option>
            <option value="2">+2</option><option value="3">+3</option>
          </select></div>
        <div class="row lw"><label>Темп</label>
          <input type="range" id="melTempo" min="50" max="200" step="5" value="100" oninput="melOnParam()"><span class="val" id="melTempoV"></span></div>
        <div class="row lw"><label>Пауза нот</label>
          <input type="range" id="melGap" min="0" max="50" step="1" value="15" oninput="melOnParam()"><span class="val" id="melGapV"></span></div>
        <div class="row lw"><label>Макс. длина</label>
          <input type="range" id="melMax" min="5" max="120" step="5" value="30" oninput="melOnParam()"><span class="val" id="melMaxV"></span></div>
        <div class="row lw"><label>Анализ аудио</label>
          <input type="range" id="melSecs" min="5" max="120" step="5" value="30" oninput="melOnSecs()"><span class="val" id="melSecsV"></span></div>
        <div class="hint" style="margin:6px 0 0">Пауза нот разделяет соседние ноты, чтобы повторы не сливались в один тон. «Анализ аудио» — сколько первых секунд аудиофайла распознавать, если отрезки не выбраны. «Макс. длина» — предел каждой готовой мелодии.</div>
      </details>
      <button class="apply-btn" id="melPrepBtn" onclick="melPrepareClick()">Подготовить</button>
      <div class="prog" id="melProg"><i id="melProgBar"></i></div>
      <div class="msg" id="melMsg"></div>
      <div class="mel-res" id="melRes">
        <div class="mel-stat" id="melStat"></div>
        <div class="hint" id="melStat2"></div>
        <div id="melPieces"></div>
        <div class="row lw"><label>Слушать как</label>
          <select class="inp" id="melPvAs" onchange="melOnPvAs()">
            <option value="brain">зуммер мозга (мелодия события)</option>
            <option value="remote">пульт, кнопка МЕЛОДИЯ</option>
          </select></div>
        <div class="fld">
          <div class="sub-lbl">Название</div>
          <input class="inp" id="melName" maxlength="48" oninput="melNameAuto=false">
        </div>
        <div class="btn-row">
          <button class="data-btn" id="melListenBtn" onclick="melListen()">Прослушать</button>
          <button class="apply-btn" id="melSaveBtn" onclick="melSave()">Сохранить в мозг</button>
          <button class="apply-btn" id="melRmBtn" onclick="melSaveRemote()">В пульт</button>
        </div>
        <div class="msg" id="melSaveMsg"></div>
      </div>
      <div class="mel-tips">
        <p>Отрезки: проведите по шкале трека — выделится кусок; несколько кусков можно склеить в одну мелодию (с паузой после каждого или без) или сохранить каждый отдельно. «Прослушать» звучит как зуммер устройства на выбранной громкости, «▶» у отрезка — оригинал.</p>
        <p>Лучше всего подходят MIDI и RTTTL рингтонов. Аудио распознаётся по высоте тона — хорошо работает с простыми одноголосыми рингтонами, хуже — с песнями и аккордами.</p>
        <p>Мелодию из фильма «Бумер» (рингтон) загрузите сами: найдите файл .mid или RTTTL-текст рингтона (например, по запросу «бумер rtttl» / «бумер midi») или mp3 рингтона, выберите его здесь и нажмите «Подготовить» → «Сохранить в мозг», затем выберите её как мелодию загрузки.</p>
      </div>
    </details>
  </div>

  <div class="page" id="page-diag">
    <div class="block">
      <div class="blk-lbl">Состояние модулей (обновляется в реальном времени)</div>
      <div class="diag-row"><span>Дисплей LCD2004 / OLED · I²C</span><span class="pill" id="dgOled">…</span></div>
      <div class="diag-row"><span>MPU-6050 (наклон) · I²C 0x68</span><span class="pill" id="dgMpu">…</span></div>
      <div class="diag-row"><span>Радио NRF24L01 · SPI</span><span class="pill" id="dgNrf">…</span></div>
      <div class="diag-row"><span>Датчик давления · <span id="dgPv">— В</span></span><span class="pill" id="dgPs">…</span></div>
      <div class="diag-row"><span>Датчик температуры</span><span class="pill" id="dgTemp">…</span></div>
      <div class="diag-row"><span>Связь с пультом</span><span class="pill" id="dgRf">…</span></div>
      <div class="diag-row"><span>Выводной вентилятор (реле GPIO25)</span><span class="pill" id="dgFans">…</span></div>
      <div class="diag-row"><span>Память мелодий (LittleFS)</span><span class="pill" id="dgFs">…</span></div>
    </div>
    <div class="block">
      <div class="blk-lbl">Что проверить</div>
      <div class="diag-hint" id="dgHint">Загрузка состояния…</div>
    </div>
    <!-- ⚡ v6.2: журнал аварий и событий -->
    <div class="block">
      <div class="blk-head"><span class="blk-lbl">Журнал событий</span><span class="blk-aux" id="logInfo"></span></div>
      <div class="log-list" id="logList"><div class="mel-empty">Загрузка…</div></div>
      <div class="btn-row two" style="margin-top:10px">
        <button class="data-btn" onclick="logLoad()">⟳ Обновить</button>
        <button class="data-btn" style="border-color:var(--danger);color:var(--danger)" onclick="logClear()">Очистить</button>
      </div>
    </div>
    <div class="block">
      <div class="blk-lbl">Подсказка по шине I²C</div>
      <div class="diag-hint">Основной дисплей — LCD2004 с I²C-переходником (0x27/0x3F), OLED (0x3C) — запасной. Дисплей и MPU-6050 используют SDA→21, SCL→22 и общую землю. LCD-модуль с питанием 5 В подключайте через преобразователь уровней I²C, чтобы на выводы ESP32 не попали 5 В.</div>
    </div>
  </div>
</div>

<script>
/*H2MEL-BEGIN*/
/* ═══════════════════════════════════════════════════════════════════════════════
   H2Mel — общий модуль мелодий веб-интерфейса мозга и Android-приложения (v6.7)
   ───────────────────────────────────────────────────────────────────────────────
   ИСХОДНИК — Финал/web_shared/h2_mel_edit.js. Копии делает tools/sync_shared_js.py:
   вставка в H2_Brain_v5/h2_web_index.h (между маркерами H2MEL) и файл
   H2_Android_App/app/src/main/assets/h2_mel_edit.js. Правьте только исходник.

   Состав:
   • play()        — прослушивание «как на зуммере»: импульсный сигнал со скважностью
                     громкости прошивки (BUZZER_VOL_DUTY), резонанс маленького зуммера,
                     огибающая музыки пульта (атака 8 мс, разрыв 20 мс, спад 10 мс);
   • pitchTrack()  — распознавание высоты тона в аудио (YIN) на заданном отрезке;
   • slice()/pieces() — нарезка «сырой» мелодии [{m,d}] на отрезки, склейка с паузами;
   • SegEditor     — редактор отрезков: волна аудио или ноты MIDI/RTTTL, выделение
                     пальцем/мышью, точная правка чисел, пауза после отрезка, прослушивание
                     оригинала;
   • uploadRemote() — загрузка нескольких мелодий в пульт по очереди с ожиданием итога.
   Без внешних библиотек: устройство работает в офлайн-сети.
   ═══════════════════════════════════════════════════════════════════════════════ */
(function (global) {
'use strict';
const H2Mel = {};

/* ── Зуммер ──────────────────────────────────────────────────────────────────── */
// Скважность ШИМ по громкости 0..10 (из 1024) — как BUZZER_VOL_DUTY в прошивках
const DUTY = [0, 6, 12, 24, 40, 64, 96, 144, 220, 340, 512];
const MUSIC_MAX_VOL = 8, MUSIC_ATTACK = 0.008, MUSIC_RELEASE = 0.010, MUSIC_GATE = 0.020;
const waveCache = new WeakMap();

// Импульсный сигнал скважности D: ряд Фурье a_n = 2·sin(nπD)/(nπ) (косинусные члены)
function pulseWave(ac, duty) {
  let m = waveCache.get(ac); if (!m) { m = new Map(); waveCache.set(ac, m); }
  if (m.has(duty)) return m.get(duty);
  const N = 64, re = new Float32Array(N), im = new Float32Array(N), D = duty / 1024;
  for (let n = 1; n < N; n++) re[n] = 2 * Math.sin(n * Math.PI * D) / (n * Math.PI);
  const w = ac.createPeriodicWave(re, im, { disableNormalization: true });
  m.set(duty, w);
  return w;
}

/* Проиграть ноты [{f,d}] «как на зуммере».
   o.vol — громкость 1..10 (как в «Звуке»); o.music — огибающая музыки пульта;
   o.onEnd — по окончании. Возвращает {stop(), t0, dur}. */
H2Mel.play = function (ac, notes, o) {
  o = o || {};
  let vol = Math.max(1, Math.min(10, o.vol || 10));
  if (o.music) vol = Math.min(vol, MUSIC_MAX_VOL);
  const osc = ac.createOscillator(), env = ac.createGain(), out = ac.createGain();
  // Маленький магнитный зуммер: резонанс ~2.7 кГц, слабые низы, завал верхов
  const res = ac.createBiquadFilter(); res.type = 'peaking'; res.frequency.value = 2700; res.Q.value = 1.4; res.gain.value = 9;
  const hp = ac.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 380;
  const lp = ac.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 9000;
  osc.setPeriodicWave(pulseWave(ac, DUTY[vol]));
  out.gain.value = 0.22;
  env.gain.value = 0;
  osc.connect(env); env.connect(res); res.connect(hp); hp.connect(lp); lp.connect(out); out.connect(ac.destination);
  const t0 = ac.currentTime + 0.05; let t = t0;
  const R = 0.004;
  for (const n of notes) {
    const d = n.d / 1000;
    if (n.f > 0) {
      osc.frequency.setValueAtTime(n.f, t);
      if (o.music) {   // как updateSound() пульта: атака, звучание до d−20 мс, спад 10 мс
        const gateAt = d > MUSIC_GATE ? d - MUSIC_GATE : d;
        const atk = Math.min(MUSIC_ATTACK, gateAt / 2), rel = Math.min(MUSIC_RELEASE, gateAt / 2);
        env.gain.setValueAtTime(0, t); env.gain.linearRampToValueAtTime(1, t + atk);
        env.gain.setValueAtTime(1, t + gateAt - rel); env.gain.linearRampToValueAtTime(0, t + gateAt);
      } else if (d > 3 * R) {
        env.gain.setValueAtTime(0, t); env.gain.linearRampToValueAtTime(1, t + R);
        env.gain.setValueAtTime(1, t + d - R); env.gain.linearRampToValueAtTime(0, t + d);
      } else { env.gain.setValueAtTime(1, t); env.gain.setValueAtTime(0, t + d); }
    }
    t += d;
  }
  osc.start(t0); osc.stop(t + 0.02);
  let done = false;
  const h = {
    t0, dur: t - t0,
    stop() { if (done) return; done = true; try { osc.onended = null; osc.stop(); } catch (e) {}
      try { osc.disconnect(); out.disconnect(); } catch (e) {} }
  };
  osc.onended = () => { if (!done) { done = true; try { out.disconnect(); } catch (e) {} if (o.onEnd) o.onEnd(); } };
  return h;
};

/* ── Сырая мелодия [{m,d}] (m — нота MIDI, m<0 — пауза; d — мс) ───────────────── */
H2Mel.duration = raw => raw.reduce((s, x) => s + (x.d > 0 ? x.d : 0), 0);

// Часть мелодии между a и b мс (ноты на границах обрезаются)
H2Mel.slice = function (raw, a, b) {
  const out = []; let t = 0;
  for (const x of raw) {
    const s = t, e = t + x.d; t = e;
    if (e <= a) continue;
    if (s >= b) break;
    const d = Math.min(e, b) - Math.max(s, a);
    if (d > 0) out.push({ m: x.m, d });
  }
  return out;
};

/* Мелодии из отрезков: segs [{a,b,gap}] мс, join — склеить в одну (паузы gap после
   каждого отрезка, кроме последнего), иначе каждый отрезок отдельно.
   rawOf(seg) — сырая мелодия отрезка. Без отрезков — одна мелодия whole(). */
H2Mel.pieces = function (segs, join, rawOf, whole) {
  if (!segs.length) return [whole()];
  const parts = segs.map(rawOf);
  if (!join) return parts;
  const one = [];
  parts.forEach((p, i) => {
    one.push(...p);
    const g = segs[i].gap | 0;
    if (g > 0 && i < parts.length - 1) one.push({ m: -1, d: g });
  });
  return [one];
};

/* ── Аудио ────────────────────────────────────────────────────────────────────── */
H2Mel.decode = function (ac, ab) {
  return new Promise((res, rej) => { const pr = ac.decodeAudioData(ab, res, rej); if (pr && pr.then) pr.then(res, rej); })
    .catch(() => { throw new Error('Не удалось декодировать аудио — этот формат не поддерживается'); });
};

// Огибающая для рисования волны: n значений 0..1 (максимум модуля по моно-сумме)
H2Mel.peaks = function (buf, n) {
  const chs = buf.numberOfChannels, len = buf.length, out = new Float32Array(n), step = len / n;
  const data = []; for (let c = 0; c < chs; c++) data.push(buf.getChannelData(c));
  let mx = 0;
  for (let i = 0; i < n; i++) {
    const s = Math.floor(i * step), e = Math.min(len, Math.floor((i + 1) * step)), inc = Math.max(1, Math.floor((e - s) / 400));
    let p = 0;
    for (let j = s; j < e; j += inc) { let v = 0; for (let c = 0; c < chs; c++) v += data[c][j]; v = Math.abs(v / chs); if (v > p) p = v; }
    out[i] = p; if (p > mx) mx = p;
  }
  if (mx > 0) for (let i = 0; i < n; i++) out[i] /= mx;
  return out;
};

/* Высота тона на отрезке [a,b] мс: моно, ~11 кГц, YIN (окно 512, шаг 12 мс, 70–2000 Гц),
   порог тишины −35 дБ, полутона, медиана по 5 кадрам, склейка, отбрасывание < 50 мс.
   Считается кусками с отдачей управления; prog(доля) — ход анализа. */
H2Mel.pitchTrack = async function (buf, a, b, prog) {
  const sr0 = buf.sampleRate, fac = Math.max(1, Math.round(sr0 / 11025)), sr = sr0 / fac, chs = buf.numberOfChannels;
  const i0 = Math.max(0, Math.floor(a / 1000 * sr0)), i1 = Math.min(buf.length, Math.floor(b / 1000 * sr0));
  const n = Math.max(0, Math.floor((i1 - i0) / fac)), ch = [];
  for (let c = 0; c < chs; c++) ch.push(buf.getChannelData(c));
  const x = new Float32Array(n), kk = 1 / (fac * chs);
  for (let i = 0; i < n; i++) { let s = 0; const o = i0 + i * fac;
    for (let c = 0; c < chs; c++) { const q = ch[c]; for (let j = 0; j < fac; j++) s += q[o + j]; } x[i] = s * kk; }
  const W = 512, tMin = Math.max(2, Math.floor(sr / 2000)), tMax = Math.min(Math.ceil(sr / 70), W >> 1), L = W - tMax;
  const hop = Math.max(1, Math.round(sr * 0.012)), nf = n >= W ? Math.floor((n - W) / hop) + 1 : 0;
  if (nf < 5) throw new Error('Отрезок аудио слишком короткий');
  const f0 = new Float32Array(nf), rms = new Float32Array(nf), d = new Float32Array(tMax + 1);
  let tY = performance.now();
  for (let fi = 0; fi < nf; fi++) {
    const o = fi * hop; let e = 0;
    for (let j = 0; j < W; j++) { const v = x[o + j]; e += v * v; }
    rms[fi] = Math.sqrt(e / W);
    if (e > 0) {
      let run = 0;   // разностная функция + кумулятивная нормировка (CMNDF)
      for (let t = 1; t <= tMax; t++) { let s = 0; for (let j = 0; j < L; j++) { const v = x[o + j] - x[o + j + t]; s += v * v; }
        run += s; d[t] = run > 0 ? s * t / run : 1; }
      let bt = -1;
      for (let t = tMin; t <= tMax; t++) if (d[t] < 0.15) { while (t < tMax && d[t + 1] < d[t]) t++; bt = t; break; }
      if (bt > 0) { let tau = bt;
        if (bt > 1 && bt < tMax) { const p = d[bt - 1], q = d[bt + 1], den = p - 2 * d[bt] + q; if (den > 1e-9) tau = bt + 0.5 * (p - q) / den; }
        const f = sr / tau; if (f >= 70 && f <= 2000) f0[fi] = f; }
    }
    if (performance.now() - tY > 30) { if (prog) prog(fi / nf); await new Promise(r => setTimeout(r, 0)); tY = performance.now(); }
  }
  let mx = 0; for (let i = 0; i < nf; i++) if (rms[i] > mx) mx = rms[i];
  const thr = mx * Math.pow(10, -35 / 20), mi = new Int16Array(nf), md = new Int16Array(nf), w = [0, 0, 0, 0, 0];
  for (let i = 0; i < nf; i++) mi[i] = (f0[i] > 0 && mx > 0 && rms[i] >= thr) ? Math.round(69 + 12 * Math.log2(f0[i] / 440)) : 0;
  for (let i = 0; i < nf; i++) { for (let k = -2; k <= 2; k++) w[k + 2] = mi[Math.min(nf - 1, Math.max(0, i + k))];
    md[i] = w.slice().sort((p, q) => p - q)[2]; }
  const fms = hop / sr * 1000, out = [];
  for (let i = 0; i < nf;) { let j = i; while (j < nf && md[j] === md[i]) j++;
    const sg = { m: md[i] > 0 ? md[i] : -1, d: (j - i) * fms }, Lp = out[out.length - 1];
    if (Lp && (sg.d < 50 || Lp.m === sg.m)) Lp.d += sg.d; else out.push(sg);
    i = j; }
  // Хвост отрезка — пауза до его конца (точная длительность при склейке)
  const tot = H2Mel.duration(out), want = b - a;
  if (want > tot) out.push({ m: -1, d: want - tot });
  return out;
};

/* ── Редактор отрезков ───────────────────────────────────────────────────────── */
const CSS = `
.h2seg{margin:10px 0 4px}
.h2seg-cv{width:100%;height:96px;display:block;border:1px solid var(--border,var(--line,#888));border-radius:8px;
  background:var(--card,var(--paper,transparent));touch-action:none;cursor:crosshair}
.h2seg-bar{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin:8px 0}
.h2seg-bar label{display:flex;gap:6px;align-items:center;font-size:13px;color:var(--text,var(--ink,inherit))}
.h2seg-row{display:grid;grid-template-columns:auto 1fr 1fr 1fr auto auto;gap:6px;align-items:center;margin:4px 0;font-size:13px}
.h2seg-row.sel{outline:2px solid var(--accent,#0af);outline-offset:2px;border-radius:6px}
.h2seg-row input{width:100%;min-width:0;padding:6px;border:1px solid var(--border,var(--line,#888));border-radius:6px;
  background:transparent;color:var(--text,var(--ink,inherit));font:inherit}
.h2seg-row span.n{font-weight:600;color:var(--accent,#0af)}
.h2seg-b{padding:6px 10px;border:1px solid var(--border,var(--line,#888));border-radius:6px;background:transparent;
  color:var(--text,var(--ink,inherit));font:inherit;font-size:13px}
.h2seg-hint{font-size:12px;color:var(--muted,#888);margin:4px 0}
.h2seg-head{display:grid;grid-template-columns:auto 1fr 1fr 1fr auto auto;gap:6px;font-size:11px;color:var(--muted,#888)}
@media (max-width:380px){.h2seg-row,.h2seg-head{grid-template-columns:auto 1fr 1fr 1fr auto}.h2seg-row .pl{display:none}}
`;
function ensureCss() {
  if (document.getElementById('h2seg-css')) return;
  const st = document.createElement('style'); st.id = 'h2seg-css'; st.textContent = CSS; document.head.appendChild(st);
}
const fmtS = ms => (ms / 1000).toFixed(1);
const MIN_SEG = 200;   // мс — минимальный отрезок

/* new SegEditor(box, {onChange, getCtx})
   setSource({kind:'audio', buffer}) | setSource({kind:'notes', raw}) | setSource(null)
   segs — [{a,b,gap}] мс; join — склеить в одну мелодию. */
class SegEditor {
  constructor(box, o) {
    ensureCss();
    this.o = o || {}; this.box = box; this.src = null; this.segs = []; this.sel = -1; this.join = true;
    this.pl = null; this.raf = 0; this.drag = null;
    box.classList.add('h2seg'); box.hidden = true;
    box.innerHTML = '<canvas class="h2seg-cv" aria-label="Шкала трека: проведите по ней, чтобы выделить отрезок"></canvas>'
      + '<div class="h2seg-hint">Проведите по шкале — выделится отрезок. Края отрезка можно тянуть. Без отрезков берётся начало трека.</div>'
      + '<div class="h2seg-bar"><button type="button" class="h2seg-b" data-a="add">+ Отрезок</button>'
      + '<button type="button" class="h2seg-b" data-a="clear">Убрать все</button>'
      + '<label><input type="radio" name="h2segj" value="1" checked> Склеить в одну</label>'
      + '<label><input type="radio" name="h2segj" value="0"> Каждый отдельно</label></div>'
      + '<div class="h2seg-head" hidden><span>№</span><span>Начало, с</span><span>Конец, с</span><span>Пауза после, мс</span><span></span><span></span></div>'
      + '<div class="h2seg-list"></div>';
    this.cv = box.querySelector('canvas'); this.list = box.querySelector('.h2seg-list'); this.head = box.querySelector('.h2seg-head');
    box.querySelector('[data-a=add]').onclick = () => this.add();
    box.querySelector('[data-a=clear]').onclick = () => { this.stopPl(); this.segs = []; this.sel = -1; this.changed(); };
    box.querySelectorAll('input[name=h2segj]').forEach(r => r.onchange = () => { this.join = r.value === '1' ? r.checked : !r.checked; this.paintList(); this.fire(); });
    this.cv.addEventListener('pointerdown', e => this.down(e));
    this.cv.addEventListener('pointermove', e => this.move(e));
    this.cv.addEventListener('pointerup', e => this.up(e));
    this.cv.addEventListener('pointercancel', e => this.up(e));
    this.ro = new ResizeObserver(() => this.draw()); this.ro.observe(this.cv);
  }
  get dur() { return this.src ? this.src.dur : 0; }
  setSource(src) {
    this.stopPl();
    this.src = null; this.segs = []; this.sel = -1; this.peaks = null;
    if (src && src.kind === 'audio') this.src = { kind: 'audio', buffer: src.buffer, dur: src.buffer.duration * 1000 };
    else if (src && src.kind === 'notes') this.src = { kind: 'notes', raw: src.raw, dur: H2Mel.duration(src.raw) };
    this.box.hidden = !this.src;
    this.changed();
  }
  // Ноты сменились (другая дорожка MIDI) — отрезки по времени сохраняются
  updateNotes(raw) { if (!this.src || this.src.kind !== 'notes') return this.setSource({ kind: 'notes', raw });
    this.src.raw = raw; this.src.dur = H2Mel.duration(raw);
    this.segs = this.segs.filter(s => s.a < this.src.dur).map(s => ({ ...s, b: Math.min(s.b, this.src.dur) })); this.changed(); }
  fire() { if (this.o.onChange) this.o.onChange(); }
  changed() { this.segs.sort((x, y) => x.a - y.a); this.paintList(); this.draw(); this.fire(); }
  add() {
    if (!this.src) return;
    const last = this.segs[this.segs.length - 1], a = last ? Math.min(last.b, this.dur - MIN_SEG) : 0;
    this.segs.push({ a: Math.max(0, a), b: Math.min(this.dur, a + 5000), gap: 0 });
    this.sel = this.segs.length - 1; this.changed();
  }
  xToMs(e) { const r = this.cv.getBoundingClientRect(); return Math.max(0, Math.min(this.dur, (e.clientX - r.left) / r.width * this.dur)); }
  hit(ms) {   // край отрезка под пальцем (в пределах 12 px) или сам отрезок
    const tol = 12 / Math.max(1, this.cv.clientWidth) * this.dur;
    for (let i = 0; i < this.segs.length; i++) { const s = this.segs[i];
      if (Math.abs(ms - s.a) <= tol) return { i, edge: 'a' };
      if (Math.abs(ms - s.b) <= tol) return { i, edge: 'b' }; }
    for (let i = 0; i < this.segs.length; i++) if (ms > this.segs[i].a && ms < this.segs[i].b) return { i, edge: null };
    return null;
  }
  down(e) {
    if (!this.src) return;
    this.cv.setPointerCapture(e.pointerId);
    const ms = this.xToMs(e), h = this.hit(ms);
    if (h && h.edge) this.drag = { i: h.i, edge: h.edge };
    else if (h) { this.sel = h.i; this.drag = null; this.paintList(); this.draw(); return; }
    else { this.segs.push({ a: ms, b: ms, gap: 0 }); this.drag = { i: this.segs.length - 1, edge: 'b', fresh: true, from: ms }; }
    this.sel = this.drag.i; this.draw();
  }
  move(e) {
    if (!this.drag) return;
    const ms = this.xToMs(e), s = this.segs[this.drag.i];
    if (this.drag.fresh) { s.a = Math.min(this.drag.from, ms); s.b = Math.max(this.drag.from, ms); }
    else if (this.drag.edge === 'a') s.a = Math.min(ms, s.b - MIN_SEG);
    else s.b = Math.max(ms, s.a + MIN_SEG);
    s.a = Math.max(0, s.a); s.b = Math.min(this.dur, s.b);
    this.draw();
  }
  up() {
    if (!this.drag) return;
    const s = this.segs[this.drag.i];
    if (this.drag.fresh && s.b - s.a < MIN_SEG) { this.segs.splice(this.drag.i, 1); this.sel = -1; }
    this.drag = null;
    const keep = this.segs[this.sel]; this.segs.sort((x, y) => x.a - y.a); this.sel = keep ? this.segs.indexOf(keep) : -1;
    this.changed();
  }
  paintList() {
    this.head.hidden = !this.segs.length;
    this.list.textContent = '';
    this.segs.forEach((s, i) => {
      const row = document.createElement('div'); row.className = 'h2seg-row' + (i === this.sel ? ' sel' : '');
      const num = document.createElement('span'); num.className = 'n'; num.textContent = i + 1;
      const inp = (v, step, set, lbl, dis) => { const x = document.createElement('input'); x.type = 'number'; x.step = step; x.min = 0;
        x.value = v; x.inputMode = 'decimal'; x.setAttribute('aria-label', lbl + ' отрезка ' + (i + 1)); x.disabled = !!dis;
        x.onchange = () => { const n = parseFloat(String(x.value).replace(',', '.')); if (isFinite(n)) set(n); this.clamp(s); this.changed(); }; return x; };
      const lastJoin = this.join && i < this.segs.length - 1;
      row.append(num,
        inp(fmtS(s.a), 0.1, n => s.a = n * 1000, 'Начало'),
        inp(fmtS(s.b), 0.1, n => s.b = n * 1000, 'Конец'),
        inp(s.gap | 0, 50, n => s.gap = Math.max(0, Math.min(10000, Math.round(n))), 'Пауза после', !lastJoin));
      const pl = document.createElement('button'); pl.type = 'button'; pl.className = 'h2seg-b pl';
      pl.textContent = this.pl && this.pl.i === i ? '■' : '▶'; pl.title = 'Прослушать оригинал отрезка';
      pl.setAttribute('aria-label', 'Прослушать оригинал отрезка ' + (i + 1)); pl.onclick = () => this.playSeg(i);
      const del = document.createElement('button'); del.type = 'button'; del.className = 'h2seg-b'; del.textContent = '✕';
      del.setAttribute('aria-label', 'Удалить отрезок ' + (i + 1));
      del.onclick = () => { if (this.pl && this.pl.i === i) this.stopPl(); this.segs.splice(i, 1); this.sel = -1; this.changed(); };
      row.append(pl, del);
      row.onclick = ev => { if (ev.target === row || ev.target === num) { this.sel = i; this.paintList(); this.draw(); } };
      this.list.appendChild(row);
    });
  }
  clamp(s) {
    s.a = Math.max(0, Math.min(s.a, this.dur - MIN_SEG));
    s.b = Math.max(s.a + MIN_SEG, Math.min(s.b, this.dur));
  }
  // Оригинал отрезка: аудио — как есть; ноты — чистым тоном
  playSeg(i) {
    if (this.pl && this.pl.i === i) { this.stopPl(); return; }
    this.stopPl();
    const s = this.segs[i], ac = this.o.getCtx && this.o.getCtx(); if (!ac || !s) return;
    let h;
    if (this.src.kind === 'audio') {
      const n = ac.createBufferSource(), g = ac.createGain(); g.gain.value = 0.8;
      n.buffer = this.src.buffer; n.connect(g); g.connect(ac.destination);
      const t0 = ac.currentTime + 0.03; n.start(t0, s.a / 1000, (s.b - s.a) / 1000);
      h = { t0, stop() { try { n.onended = null; n.stop(); n.disconnect(); } catch (e) {} } };
      n.onended = () => { if (this.pl && this.pl.h === h) this.stopPl(); };
    } else {
      const notes = H2Mel.slice(this.src.raw, s.a, s.b).map(x => ({ f: x.m >= 0 ? Math.round(440 * Math.pow(2, (x.m - 69) / 12)) : 0, d: Math.max(1, Math.round(x.d)) }));
      h = H2Mel.play(ac, notes, { vol: 10, onEnd: () => { if (this.pl && this.pl.h === h) this.stopPl(); } });
    }
    this.pl = { i, h, a: s.a, b: s.b, ac };
    this.paintList(); this.tick();
  }
  stopPl() { if (!this.pl) return; const p = this.pl; this.pl = null; p.h.stop(); cancelAnimationFrame(this.raf); this.paintList(); this.draw(); }
  tick() { this.draw(); if (this.pl) this.raf = requestAnimationFrame(() => this.tick()); }
  draw() {
    const cv = this.cv, w = cv.clientWidth, hh = cv.clientHeight; if (!w || !this.src) return;
    const dpr = global.devicePixelRatio || 1; if (cv.width !== Math.round(w * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(hh * dpr); }
    const g = cv.getContext('2d'); g.setTransform(dpr, 0, 0, dpr, 0, 0); g.clearRect(0, 0, w, hh);
    const css = getComputedStyle(this.box), col = n => css.getPropertyValue(n).trim();
    const acc = col('--accent') || '#0af', fg = col('--text') || col('--ink') || '#888', mut = col('--muted') || '#888';
    const X = ms => ms / this.dur * w;
    // отрезки
    this.segs.forEach((s, i) => { g.fillStyle = acc; g.globalAlpha = i === this.sel ? 0.30 : 0.18; g.fillRect(X(s.a), 0, X(s.b) - X(s.a), hh);
      g.globalAlpha = 1; g.fillRect(X(s.a) - 1, 0, 2, hh); g.fillRect(X(s.b) - 1, 0, 2, hh);
      g.fillStyle = acc; g.font = '600 11px sans-serif'; g.fillText(String(i + 1), X(s.a) + 4, 12); });
    // содержимое
    g.fillStyle = fg; g.globalAlpha = 0.75;
    if (this.src.kind === 'audio') {
      const n = Math.max(32, Math.floor(w)); if (!this.peaks || this.peaks.length !== n) this.peaks = H2Mel.peaks(this.src.buffer, n);
      const mid = hh / 2 + 6, amp = (hh - 22) / 2;
      for (let i = 0; i < n; i++) { const v = Math.max(0.5, this.peaks[i] * amp); g.fillRect(i * w / n, mid - v, Math.max(1, w / n - 0.5), 2 * v); }
    } else {
      let lo = 127, hi = 0; for (const x of this.src.raw) if (x.m >= 0) { lo = Math.min(lo, x.m); hi = Math.max(hi, x.m); }
      const span = Math.max(12, hi - lo), top = 18, ah = hh - top - 6; let t = 0;
      for (const x of this.src.raw) { if (x.m >= 0) { const y = top + ah - (x.m - lo) / span * ah; g.fillRect(X(t), y - 1.5, Math.max(1, X(t + x.d) - X(t) - 0.5), 3); } t += x.d; }
    }
    g.globalAlpha = 1;
    // шкала времени
    g.fillStyle = mut; g.font = '10px sans-serif';
    const stepS = [1, 2, 5, 10, 15, 30, 60, 120].find(v => this.dur / 1000 / v <= w / 60) || 300;
    for (let s = 0; s * 1000 <= this.dur; s += stepS) { const x = X(s * 1000); g.fillRect(x, hh - 4, 1, 4); if (x < w - 24) g.fillText(s + ' с', x + 2, hh - 6); }
    // позиция воспроизведения
    if (this.pl) { const p = Math.min(this.pl.b, this.pl.a + Math.max(0, this.pl.ac.currentTime - this.pl.h.t0) * 1000);
      g.fillStyle = acc; g.fillRect(X(p) - 1, 0, 2, hh); }
  }
}
H2Mel.SegEditor = SegEditor;

/* ── Загрузка в пульт ────────────────────────────────────────────────────────── */
/* files — [{name, bytes}] (готовые H2M1). upload(bytes) → Promise<{ok,err}> (POST /rmel/up),
   cfg() → Promise<объект /rmel/cfg>. Мелодии идут по одной: следующая — после итога
   предыдущей («done»/«err»). onStep(текст) — ход загрузки. Возвращает число загруженных. */
H2Mel.uploadRemote = async function (files, upload, cfg, onStep) {
  let ok = 0;
  for (let i = 0; i < files.length; i++) {
    const f = files[i], tag = files.length > 1 ? (i + 1) + '/' + files.length + ' «' + f.name + '»' : '«' + f.name + '»';
    onStep('Отправка ' + tag + ' в мозг…');
    const r = await upload(f.bytes);
    if (!r || !r.ok) throw new Error((r && r.err) || 'мозг не принял мелодию');
    const t0 = Date.now(); let seen = false;
    for (;;) {
      await new Promise(res => setTimeout(res, 400));
      const c = await cfg(), j = c.job || {};
      if (j.st === 'send' || j.st === 'check') { seen = true; onStep('Передача ' + tag + ' в пульт… ' + (j.pct | 0) + ' %'); }
      else if (j.st === 'done' && (seen || Date.now() - t0 > 800)) break;
      else if (j.st === 'err' && (seen || Date.now() - t0 > 800)) throw new Error(j.err || 'ошибка передачи');
      if (Date.now() - t0 > 60000) throw new Error('пульт не ответил за минуту');
    }
    ok++;
  }
  return ok;
};

global.H2Mel = H2Mel;
})(window);
/*H2MEL-END*/
</script>
<script>
/* ══════════ ⚡ v6.3: ПОМОЩНИКИ DOM ══════════
   Статус приходит раз в секунду. Элементы кэшируются, а в DOM пишется только то,
   что реально изменилось: меньше перерасчётов раскладки на слабых телефонах.
   Этот блок — первым: код темы ниже вызывается сразу при загрузке. */
const elCache={};
function byId(id){ let e=elCache[id]; if(!e||e.isConnected===false){ e=document.getElementById(id); if(e) elCache[id]=e; } return e; }
function txt(id,v){ const e=byId(id); v=String(v); if(e&&e.textContent!==v) e.textContent=v; }
function cn(id,c){ const e=byId(id); if(e&&e.className!==c) e.className=c; }
function sty(id,k,v){ const e=byId(id); if(e&&e.style[k]!==v) e.style[k]=v; }
// Значение поля из статуса — только если пользователь его сейчас не редактирует
function fld(id,v){ const e=byId(id); v=String(v); if(e&&e.value!==v&&document.activeElement!==e) e.value=v; }
// Установить значение ползунка и закрасить дорожку (--pct) по его min/max
function setRng(e,v){ if(!e) return; if(e.value!==String(v)) e.value=v; const mn=+e.min, mx=+e.max;
  e.style.setProperty('--pct',((+e.value-mn)/((mx-mn)||1)*100).toFixed(1)+'%'); }
function esc(s){ return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }
function netErr(e){ return (e&&e.name==='AbortError')?'нет ответа от мозга':(e instanceof TypeError)?'нет связи с мозгом':((e&&e.message)||'ошибка'); }
// ⚡ v6.3: запрос к мозгу с тайм-аутом (по умолчанию 8 с); не-2xx → ошибка с полем status
function getJson(u,ms){
  const c=window.AbortController?new AbortController():null;
  if(c) setTimeout(()=>c.abort(),ms||8000);
  return fetch(u,c?{signal:c.signal}:{}).then(r=>{ if(!r.ok){
      const e=new Error(r.status===503?'мозг занят — повторите':'ошибка '+r.status); e.status=r.status; throw e; }
    return r; });
}
function flash(id,msg,bad){ const e=byId(id); if(!e) return; e.textContent=msg;
  e.style.color=bad?'var(--danger)':''; e.style.opacity='1';
  clearTimeout(e._ft); e._ft=setTimeout(()=>e.style.opacity='0',bad?5000:2500); }
function fmtUp(s){ return [Math.floor(s/3600),Math.floor(s%3600/60),s%60].map(n=>String(n).padStart(2,'0')).join(':'); }
function fmtMS(s){ s=Math.max(0,s|0); const h=Math.floor(s/3600),m=Math.floor(s%3600/60),c=s%60;
  return (h?h+':'+String(m).padStart(2,'0'):m)+':'+String(c).padStart(2,'0'); }
function fmtNL(v){ return v<0?'—':(v>=100?v.toFixed(0):v>=10?v.toFixed(1):v.toFixed(2))+' нл'; }

/* ══════════ ИСТОРИЯ (на телефоне) ══════════
   Хранится в localStorage телефона. Структура: массив {ts,p,t}.
   ⚡ v6.3: максимумы и размер базы ведутся при записи, а не пересчитываются
   по всей базе на каждое сообщение. */
let history=[], histBytes=0, histSaveOk=true, histPMax=-Infinity, histTMax=-Infinity, lastHistPush=0;
const HIST_KEY='h2pro_history';
const MAX_POINTS=20000;       // ~ неделя при записи раз в 30 сек
const isNum=v=>v!=null&&v!==''&&isFinite(+v);   // null/'' — не 0
function histOk(r){ return !!r&&isNum(r.ts)&&+r.ts>0&&isNum(r.p); }
function histNorm(r){ return {ts:+r.ts,p:+r.p,t:isNum(r.t)?+r.t:null}; }
function histStats(){ histPMax=histTMax=-Infinity;
  for(const r of history){ if(r.p>histPMax) histPMax=r.p; if(r.t!=null&&r.t>histTMax) histTMax=r.t; } }
function loadHistory(){
  try{ const s=localStorage.getItem(HIST_KEY), a=s?JSON.parse(s):[];
    history=Array.isArray(a)?a.filter(histOk).map(histNorm):[]; histBytes=s?s.length:0; }
  catch(e){ history=[]; histSaveOk=false; }
  histStats();
}
function saveHistory(){
  try{ const s=JSON.stringify(history); histBytes=s.length; localStorage.setItem(HIST_KEY,s); histSaveOk=true; }
  catch(e){ histSaveOk=false; }
}
function pushHistory(p,t){
  const now=Date.now();
  if(now-lastHistPush < 30000) return;   // не чаще раза в 30 сек
  lastHistPush=now;
  const r={ts:now,p:+p.toFixed(2),t:+t.toFixed(1)};
  history.push(r);
  if(history.length>MAX_POINTS){ history=history.slice(-MAX_POINTS); histStats(); }
  else{ if(r.p>histPMax) histPMax=r.p; if(r.t>histTMax) histTMax=r.t; }
  saveHistory(); updateDbInfo();
}
function updateDbInfo(){
  txt('dbInfo','Точек: '+history.length+' · Размер: '+(histBytes/1024).toFixed(1)+' КБ · Авто-сохранение: '+(histSaveOk?'вкл':'недоступно'));
  txt('histInfo','Точек в базе: '+history.length);
}

/* ══════════ ТЕМА ══════════ */
function toggleTheme(){
  const light=document.body.classList.toggle('light');
  txt('themeBtn',light?'☀':'🌙');
  try{ localStorage.setItem('h2pro_theme',light?'light':'dark'); }catch(e){}   // ⚡ v6.3: хранилище может быть запрещено
  if(byId('page-graph').classList.contains('active')) drawCharts();          // цвета сетки — из темы
}
// ⚡ v6.3: хранилище может быть запрещено (окно входа в сеть, запрет данных сайтов) — не ронять скрипт
try{ if(localStorage.getItem('h2pro_theme')==='light'){ document.body.classList.add('light'); txt('themeBtn','☀'); } }catch(e){}

/* ══════════ ВКЛАДКИ ══════════ */
function showPage(p,btn){
  document.querySelectorAll('.page').forEach(e=>e.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(e=>e.classList.remove('active'));
  byId('page-'+p).classList.add('active');
  btn.classList.add('active');
  if(p==='graph') drawCharts();
  if(p==='sound'){ sndLoadCfg(); rmLoad(true); }   // ⚡ v6.0: свежий список мелодий и настроек звука; ⚡ v6.7: мелодии пульта
  if(p==='diag')  logLoad();      // ⚡ v6.2: журнал событий
}

/* ══════════ WEBSOCKET + РЕЗЕРВНЫЙ ОПРОС ══════════
   ⚡ v6.3: lastMsg — любое сообщение WS, lastData — последние разобранные данные
   (WS или /status). «Зомби»-сокет (мозг перезагрузился, обрыв без FIN) пересоздаётся;
   при отсутствии свежих данных — баннер «нет связи» и приглушённые показания. */
let st={};
let ws=null, wsAlive=false, wsT=0, lastMsg=0, lastData=0, polling=false;
const pageT0=Date.now();
function onStatus(d){
  if(typeof d==='string'){ try{ d=JSON.parse(d); }catch(e){ return; } }
  if(!d||d.p===undefined) return;   // {"err":...} — не данные
  st=d; lastData=Date.now();
  try{ render(); }catch(e){ console.error(e); }
}
function wsDrop(){ const o=ws; ws=null; wsAlive=false;
  if(o){ o.onopen=o.onclose=o.onerror=o.onmessage=null; try{ o.close(); }catch(e){} }
  byId('wsDot').classList.remove('live'); }
function wsConnect(){
  if(ws&&ws.readyState<2) return;   // ⚡ v6.3: уже подключён или подключается
  let s; try{ s=new WebSocket('ws://'+location.host+'/ws'); }catch(e){ setTimeout(wsConnect,5000); return; }
  ws=s; wsT=Date.now();
  s.onopen=()=>{ wsAlive=true; lastMsg=Date.now(); staleTick(); };   // свежий сокет — не «устаревший»
  s.onclose=()=>{ if(ws===s){ ws=null; wsAlive=false; staleTick(); } setTimeout(wsConnect,2000); };
  s.onerror=()=>{ try{ s.close(); }catch(e){} };
  s.onmessage=ev=>{ lastMsg=Date.now(); onStatus(ev.data); };
}
function staleTick(){
  const age=Date.now()-(lastData||pageT0), s=age>5000;
  document.body.classList.toggle('stale',s);
  byId('wsDot').classList.toggle('live',wsAlive&&!s);   // зелёная точка не врёт при полуоткрытом WS
  cn('linkBar','alert-bar danger'+(s?' show':''));
  txt('linkBar',!s?'':lastData?'⚠ НЕТ СВЯЗИ С МОЗГОМ — данные '
    +(age<120000?Math.round(age/1000)+' с':Math.round(age/60000)+' мин')+' назад':'⚠ НЕТ СВЯЗИ С МОЗГОМ');
}
// Команда: по WS только при живом сокете, иначе HTTP с проверкой ответа (503 — очередь занята)
function cmd(id){
  if(wsAlive&&ws&&ws.readyState===1&&Date.now()-lastMsg<3000){ try{ ws.send('{"cmd":'+id+'}'); return; }catch(e){} }
  getJson('/cmd?id='+id).catch(e=>flash('cmdMsg','✗ команда не доставлена: '+netErr(e),true));
}

/* ══════════ УПРАВЛЕНИЕ ДАВЛЕНИЕМ ══════════ */
let pendingTgt=1.0, purging=false, pwrT=null, qBtns=null;
let tgtEdit=0;   // ⚡ v6.3: момент последней правки цели; пока < 20 с — render не трогает ползунок
function tgtPaint(v){ setRng(byId('tslider'),v); txt('tval',v.toFixed(1)); updateQ(v); }
function onSlide(v){ v=+v; tgtEdit=Date.now(); pendingTgt=v; tgtPaint(v); }
function setQ(v){ onSlide(v); }
function stepTgt(d){ setQ(Math.max(0.5,Math.min(6.0,Math.round((pendingTgt+d)*2)/2))); }
function updateQ(v){ (qBtns||(qBtns=document.querySelectorAll('#page-main .qb'))).forEach((b,i)=>b.classList.toggle('active',(i+1)===v)); }
function applyTgt(){ const v=pendingTgt.toFixed(1);   // значение — до запроса: render может сменить pendingTgt
  getJson('/setpressure?val='+v).then(()=>{ tgtEdit=0; flash('tok','✓ установлено: '+v+' бар'); })
    .catch(e=>flash('tok','✗ '+netErr(e),true)); }
function handlePower(){ if(purging) return;
  if(!pwrT) pwrT=setTimeout(()=>{ pwrT=null; cmd(1); },380);
  else{ clearTimeout(pwrT); pwrT=null; cmd(99); } }

/* ══════════ ⚡ v6.2: ТАЙМЕР, ОБСЛУЖИВАНИЕ, ГАЗ, ЖУРНАЛ, ЧАСЫ ══════════ */
let tmrDirty=false, mntDirty=false, gasDirty=false, logSeen=-1, lastSyncTry=0;
function tmrPaint(){ sty('tmrMin','display',+byId('tmrSel').value<0?'':'none'); }
function tmrSync(min){ const s=byId('tmrSel'), a=document.activeElement;
  if(tmrDirty||a===s||a===byId('tmrMin')) return;
  const opt=[...s.options].some(o=>+o.value===min), v=opt?String(min):'-1';
  if(s.value!==v) s.value=v;
  if(!opt) fld('tmrMin',min); tmrPaint(); }
function tmrApply(){ const sel=+byId('tmrSel').value; let v=sel;
  if(sel<0){ v=Math.round(+byId('tmrMin').value);   // ⚡ v6.3: пустое поле или 0 не снимают ограничение
    if(!(v>=1&&v<=600)){ flash('tmrOk','✗ от 1 до 600 мин',true); byId('tmrMin').focus(); return; } }
  if(!(v>=0&&v<=600)) return;
  getJson('/settimer?min='+v).then(()=>{ tmrDirty=false; flash('tmrOk','✓ '+(v?v+' мин':'без ограничения')); })
    .catch(e=>flash('tmrOk','✗ '+netErr(e),true)); }
function mntApply(){ const h=Math.round(+byId('mntI').value);
  if(!(h>=1&&h<=1000)){ flash('mntOk','✗ от 1 до 1000 ч',true); return; }
  getJson('/maint?h='+h).then(()=>{ mntDirty=false; flash('mntOk','✓ сохранено'); }).catch(e=>flash('mntOk','✗ '+netErr(e),true)); }
function mntDone(){ getJson('/maint?done=1').then(()=>flash('mntOk','✓ отмечено, отсчёт заново')).catch(e=>flash('mntOk','✗ '+netErr(e),true)); }
function gasApply(){ const v=+byId('gTank').value;
  if(!(v>=0.1&&v<=100)){ flash('gOk','✗ от 0,1 до 100 л',true); return; }
  getJson('/setgas?tank='+v.toFixed(2)).then(()=>{ gasDirty=false; flash('gOk','✓ сохранено'); }).catch(e=>flash('gOk','✗ '+netErr(e),true)); }
function gasReset(){ if(confirm('Обнулить счётчик газа?')) getJson('/setgas?reset=1').then(()=>flash('gOk','✓ обнулено')).catch(e=>flash('gOk','✗ '+netErr(e),true)); }
// Журнал: коды событий мозга (h2_events.h)
const EVT={1:['Загрузка',''],2:['Авария: давление','bad'],3:['Авария: наклон','bad'],4:['Перегрев','bad'],
  5:['Бак не набирает давление','warn'],6:['Датчик давления неисправен','bad'],7:['Датчик давления в норме',''],
  8:['Нет данных датчика температуры','warn'],9:['Подозрение на утечку','warn'],10:['Утечка — система выключена','bad'],
  11:['Таймер выключил систему',''],12:['Пора обслуживать','warn'],13:['Обслуживание отмечено',''],14:['Часы синхронизированы',''],
  15:['Утечка: частая подкачка при удержании','warn'],
  16:['Датчик наклона не отвечает — MOSFET заблокирован','bad'],17:['Продувка не завершена','bad'],   // ⚡ v6.3
  18:['Калибровка датчика давления сброшена — откалибруйте заново','bad']};   // ⚡ v6.5
const RST=['?','питание','внешний','программный','паника','прерывание WDT','задача WDT','WDT','глубокий сон','просадка питания','SDIO'];
function evtText(r){ const e=EVT[r.c]||['Событие '+r.c,'']; let m=e[0];
  if(r.c===1) m+=' ('+(RST[r.v]||r.v)+')';
  else if(r.c===2||r.c===5||r.c===17) m+=' · '+(r.v/100).toFixed(2)+' бар';
  else if(r.c===3) m+=' · '+r.v+'°'; else if(r.c===4) m+=' · '+r.v+'°C';
  else if(r.c===6||r.c===7) m+=' · '+(r.v/1000).toFixed(2)+' В';
  else if(r.c===9||r.c===10) m+=' · −'+(r.v/100).toFixed(2)+' бар/30 с';
  else if(r.c===15) m+=' · MOSFET '+r.v+'% времени';
  else if(r.c===11) m+=' · '+r.v+' мин'; else if(r.c===12) m+=' · '+r.v+' ч';
  return [m,e[1]]; }
function evtTime(r,j){ if(r.e){ const d=new Date(r.e*1000);
    return d.toLocaleDateString('ru-RU',{day:'2-digit',month:'2-digit'})+' '+d.toLocaleTimeString('ru-RU',{hour:'2-digit',minute:'2-digit'}); }
  return (r.b===j.boot?'':'#'+r.b+' ')+'+'+fmtUp(r.u); }
function logErr(e){ byId('logList').innerHTML='<div class="mel-empty">'+esc(netErr(e))+'</div>'; }
function logLoad(){ getJson('/log').then(r=>r.json()).then(j=>{
    txt('logInfo',j.list.length+' записей');
    byId('logList').innerHTML=j.list.length?j.list.map(r=>{ const t=evtText(r);
      return `<div class="log-row ${t[1]}"><span class="log-t">${esc(evtTime(r,j))}</span><span class="log-m">${esc(t[0])}</span></div>`; }).join('')
      :'<div class="mel-empty">Событий нет</div>'; })
  .catch(logErr); }
function logClear(){ if(confirm('Очистить журнал событий?')) getJson('/log/clear').then(logLoad).catch(logErr); }
// Часы мозга — с этого устройства (своего RTC у мозга нет). ⚡ v6.3: повтор из render, пока tsync=0
function syncTime(){ lastSyncTry=Date.now(); fetch('/settime?t='+Math.floor(Date.now()/1000)).catch(()=>{}); }

/* ══════════ ВЫВОДНОЙ ВЕНТИЛЯТОР НА РЕЛЕ ══════════
   Режимы: 0 Авто (по температуре), 1 Вкл, 2 Выкл, 3 При работе.
   Значения подтягиваются из статуса, пока пользователь не начал правку (dirty);
   после ПРИМЕНИТЬ флаг сбрасывается и карточка снова следует за мозгом. */
const FAN_MODES=['Авто','Вкл','Выкл','При работе'];
const FAN_HINT=['По температуре; без датчика и при перегреве — включён.',
  'Работает постоянно.',
  'Выключен всегда — даже при перегреве.',
  'Пока система включена или идёт продувка, иначе — по температуре.'];
const fanUi={2:{m:3,on:45,off:40,dirty:false,init:false}};
function fanCardHtml(i,title){
  return `<div class="blk-head"><span class="blk-lbl">${title}</span><span class="pill" id="fanSt${i}">—</span></div>
    <div class="seg">${FAN_MODES.map((n,m)=>`<button class="seg-opt" id="fan${i}m${m}"${m===3?' style="flex:1.45"':''} onclick="fanSetMode(${i},${m})">${n}</button>`).join('')}</div>
    <div class="hint" id="fanHint${i}"></div>
    <div id="fanThr${i}">
      <div class="row lw"><label>Включать при</label><input type="range" id="fanOn${i}" min="20" max="90" step="1" oninput="fanOnIn(${i},this.value)"><span class="val" id="fanOnV${i}"></span></div>
      <div class="row lw"><label>Выключать при</label><input type="range" id="fanOff${i}" min="15" max="89" step="1" oninput="fanOffIn(${i},this.value)"><span class="val" id="fanOffV${i}"></span></div>
    </div>
    <button class="apply-btn" onclick="fanApply(${i})">Применить</button>
    <div class="ok-msg" id="fanOk${i}"></div>`;
}
function fanPaint(i){
  const u=fanUi[i];
  for(let m=0;m<4;m++) cn('fan'+i+'m'+m,'seg-opt'+(m===u.m?' active':''));
  txt('fanHint'+i,FAN_HINT[u.m]||'');
  sty('fanThr'+i,'display',(u.m===0||u.m===3)?'':'none');   // пороги нужны только Авто / При работе
  const on=byId('fanOn'+i), off=byId('fanOff'+i);
  if(on){ setRng(on,u.on); txt('fanOnV'+i,u.on+' °C'); }
  if(off){ off.max=u.on-1; setRng(off,u.off); txt('fanOffV'+i,u.off+' °C'); }
}
function fanSetMode(i,m){ fanUi[i].m=m; fanUi[i].dirty=true; fanPaint(i); }
function fanOnIn(i,v){ const u=fanUi[i]; u.on=+v; if(u.off>u.on-1) u.off=u.on-1; u.dirty=true; fanPaint(i); }
function fanOffIn(i,v){ const u=fanUi[i]; u.off=Math.min(+v,u.on-1); u.dirty=true; fanPaint(i); }
function fanApply(i){ const u=fanUi[i];
  getJson('/setfans?i='+i+'&mode='+u.m+'&ton='+u.on+'&toff='+u.off)
    .then(()=>{ u.dirty=false; flash('fanOk'+i,'✓ применено'); })
    .catch(e=>flash('fanOk'+i,'✗ '+netErr(e),true)); }
function fanSync(i,d){
  const k='f'+i, u=fanUi[i];
  if(u.dirty || d[k+'m']===undefined) return;
  const m=+d[k+'m'], on=Math.round(d[k+'on']), off=Math.round(d[k+'off']);
  if(u.init && u.m===m && u.on===on && u.off===off) return;
  u.m=m; u.on=on; u.off=off; u.init=true; fanPaint(i);
}
function fanState(i,on){ const s=on?'ВКЛ':'ВЫКЛ';
  cn('fanL'+i,'fan-ln'+(on?' on':'')); txt('fanTx'+i,s);
  cn('fanSt'+i,'pill'+(on?' ok':'')); txt('fanSt'+i,s);
  cn('dgFans','pill'+(on?' ok':'')); txt('dgFans',s); }

/* ══════════ КАЛИБРОВКА ══════════
   ⚡ v6.3: значения берутся из статуса мозга (pco/pcs), пока пользователь не начал
   правку (calDirty). Ползунок только показывает — его value браузер округляет до
   шага, поэтому отправляется calCfg (без правки — ровно то, что стоит в мозге).
   ⚡ v6.3: калибровка сдвигает аварийную защиту по давлению (ноль, снятый под давлением,
   прячет реальное давление). Поэтому «0 бар» и сохранение — только при выключенной системе
   без продувки и с подтверждением (то же правило, что saveCal в Android), а несохранённая
   правка живёт CAL_TTL без действий — потом значения снова берутся из мозга, чтобы старый
   offset не ушёл в мозг вместе с поздней правкой масштаба. */
const CAL_TTL=60000;
let calDirty=false, calLoaded=false, calEditT=0;
let calCfg={off:0.5,scl:2.5};
function calPaint(){ setRng(byId('calOffSlider'),calCfg.off); setRng(byId('calSclSlider'),calCfg.scl);
  txt('calOffVal',calCfg.off.toFixed(3)+' В'); txt('calSclVal',calCfg.scl.toFixed(2)); }
function calEdit(){ calDirty=true; calEditT=Date.now(); calPaint(); }
function onCalOff(v){ calCfg.off=+v; calEdit(); }
function onCalScl(v){ calCfg.scl=+v; calEdit(); }
function calSync(d){
  if(calDirty&&Date.now()-calEditT>CAL_TTL){ calDirty=false;
    flash('calOk','несохранённая правка калибровки отменена (1 мин без сохранения)',true); }
  if(d.pco===undefined||(calDirty&&calLoaded)) return;
  const o=+d.pco, s=+d.pcs; calLoaded=true;
  if(calDirty||o!==calCfg.off||s!==calCfg.scl){ calDirty=false; calCfg.off=o; calCfg.scl=s; calPaint(); } }
function calFresh(d){ return d.pv!==undefined&&lastData&&Date.now()-lastData<=5000; }
// Система включена / продувка / электролиз — бак может быть под давлением, калибровку не трогаем
function calBusy(d){ return d.pwr||d.prg||d.psh||d.ele; }
// «0 бар»: текущее напряжение датчика → offset (сохраняется только кнопкой СОХРАНИТЬ)
function calZero(){ const d=st;
  if(!calFresh(d)){ flash('calOk','✗ нет свежих данных от мозга',true); return; }
  if(d.psf){ flash('calOk','✗ датчик давления неисправен — калибровка невозможна',true); return; }
  if(calBusy(d)){ flash('calOk','✗ выключите систему и стравите бак до 0 бар (дождитесь конца продувки)',true); return; }
  const v=Math.round(+d.pv*1000)/1000;
  if(!(v>=0&&v<=2)){ flash('calOk','✗ напряжение вне диапазона 0–2 В',true); return; }
  if(!confirm('Бак стравлен до 0 бар (продувка завершена)?\n\nНапряжение датчика '+v.toFixed(3)+' В (сейчас это '+
    (+d.p).toFixed(2)+' бар) станет точкой 0 бар.\nНеверный ноль сдвигает аварийную защиту по давлению.')) return;
  calCfg.off=v; calEdit(); flash('calOk','offset '+v.toFixed(3)+' В — нажмите «Сохранить» (в течение минуты)'); }
function applyCal(){
  if(!calLoaded){ flash('calOk','✗ калибровка ещё не прочитана из мозга — дождитесь связи',true); return; }
  const d=st;
  if(!calFresh(d)){ flash('calOk','✗ нет свежих данных от мозга',true); return; }
  if(calBusy(d)){ flash('calOk','✗ сначала выключите систему и дождитесь конца продувки',true); return; }
  // Отправляем ровно то, что показали в подтверждении
  const off=+calCfg.off.toFixed(3), scl=+calCfg.scl.toFixed(3);
  if(!(off>=0&&off<=2)||!(scl>=0.1&&scl<=10)){ flash('calOk','✗ offset 0–2 В, масштаб 0,1–10 бар/В',true); return; }
  if(!confirm('Сохранить калибровку датчика давления?\n\n'+
    'Сейчас в мозге: '+(+d.pco).toFixed(3)+' В · '+(+d.pcs).toFixed(3)+' бар/В\n'+
    'Новая: '+off.toFixed(3)+' В · '+scl.toFixed(3)+' бар/В\n'+
    'Давление: сейчас '+(+d.p).toFixed(2)+' бар → станет '+Math.max(0,(+d.pv-off)*scl).toFixed(2)+' бар\n\n'+
    'Аварийная защита по давлению работает по этим значениям — неверная калибровка опасна.')) return;
  calEditT=Date.now();   // пока ждали подтверждения, правка не должна «протухнуть»
  getJson('/calibrate?offset='+off.toFixed(3)+'&scale='+scl.toFixed(3))
    .then(()=>{ calDirty=false; flash('calOk','✓ калибровка сохранена'); })
    .catch(e=>flash('calOk','✗ '+netErr(e),true)); }

/* ══════════ СБРОСЫ ══════════ */
function resetRT(){ if(confirm('Обнулить счётчик наработки?'))
  getJson('/resetruntime').then(()=>flash('rtOk','✓ обнулено')).catch(e=>flash('rtOk','✗ '+netErr(e),true)); }
function resetCycles(){ if(confirm('Обнулить счётчики циклов реле?'))
  getJson('/resetcycles').then(()=>flash('cycOk','✓ обнулено')).catch(e=>flash('cycOk','✗ '+netErr(e),true)); }

/* ══════════ ЭКСПОРТ/ИМПОРТ ══════════ */
function download(name,content,type){
  const blob=new Blob([content],{type:type}), a=document.createElement('a'), url=URL.createObjectURL(blob);
  a.href=url; a.download=name;
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
  setTimeout(()=>URL.revokeObjectURL(url),10000);   // ⚡ v6.3: освободить; не сразу — скачивание на телефоне асинхронное
}
function exportCSV(){
  let csv='timestamp,iso,pressure_bar,temp_c\n';
  history.forEach(r=>{ csv+=r.ts+','+new Date(r.ts).toISOString()+','+r.p+','+r.t+'\n'; });
  download('h2pro_'+Date.now()+'.csv',csv,'text/csv');
}
function exportJSON(){ download('h2pro_'+Date.now()+'.json',JSON.stringify(history),'application/json'); }
function importData(inp){
  const f=inp.files[0]; if(!f)return;
  const rd=new FileReader();
  rd.onload=e=>{
    try{
      const s=String(e.target.result); let data;
      // ⚡ v6.3: расширение без учёта регистра или JSON по содержимому
      if(/\.json$/i.test(f.name)||/^\s*[\[{]/.test(s)) data=JSON.parse(s);
      else{ // CSV
        data=[]; const lines=s.split('\n');
        for(let i=1;i<lines.length;i++){ const c=lines[i].split(','); if(c.length>=4) data.push({ts:+c[0],p:+c[2],t:+c[3]}); }
      }
      if(!Array.isArray(data)) throw new Error('файл не содержит массив записей');
      // ⚡ v6.3: только корректные записи (ts>0, числовое p; t — число или null)
      const n0=data.length; data=data.filter(histOk).map(histNorm);
      if(!data.length){ alert('Нет корректных записей'+(n0?' (пропущено '+n0+')':'')); return; }
      // объединяем с текущей базой, сортируем по времени, убираем дубли
      const map={}; history.concat(data).forEach(r=>{ if(r&&r.ts) map[r.ts]=r; });
      history=Object.values(map).sort((a,b)=>a.ts-b.ts);
      if(history.length>MAX_POINTS) history=history.slice(-MAX_POINTS);
      histStats(); saveHistory(); updateDbInfo(); drawCharts();
      alert('Импортировано: '+data.length+(n0>data.length?' (пропущено '+(n0-data.length)+')':'')+'. Всего точек: '+history.length);
    }catch(err){ alert('Ошибка импорта: '+((err&&err.message)||err)); }
  };
  rd.readAsText(f); inp.value='';
}
function clearHistory(){ if(confirm('Очистить всю базу истории на телефоне?')){ history=[]; histStats(); saveHistory(); updateDbInfo(); drawCharts(); } }

/* ══════════ ГРАФИКИ ══════════ */
let chartRange='1h', rszT=0;
function setRange(r,btn){ chartRange=r;
  document.querySelectorAll('#page-graph .range-btn').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active'); drawCharts(); }
function rangeMs(){ return chartRange==='1h'?3600e3:chartRange==='1d'?86400e3:604800e3; }
// Буфер холста — 2× CSS-размера; поле слева L под подписи оси
function drawLine(cv,data,key,color,maxY){
  const cw=cv.clientWidth, ch=cv.clientHeight;
  if(!cw||!ch) return;   // ⚡ v6.3: вкладка скрыта — пропуск, showPage('graph') перерисует
  const ctx=cv.getContext('2d'), w=cv.width=cw*2, h=cv.height=ch*2, L=56;
  const css=getComputedStyle(document.body), mut=css.getPropertyValue('--muted');   // ⚡ v6.3: подписи цветом --muted (контраст ~6:1 вместо ~1.7:1)
  ctx.strokeStyle=css.getPropertyValue('--border'); ctx.lineWidth=1;
  ctx.fillStyle=mut; ctx.font='20px monospace'; ctx.textAlign='right';
  for(let i=0;i<=4;i++){ const y=i*(h-20)/4+10; ctx.beginPath(); ctx.moveTo(L,y); ctx.lineTo(w,y); ctx.stroke();
    ctx.fillText((maxY*(1-i/4)).toFixed(1),L-6,y+7); }
  if(data.length<2){ ctx.font='22px monospace'; ctx.textAlign='center';
    ctx.fillText('Накопление данных...',w/2,h/2); return; }
  const t0=data[0].ts, t1=data[data.length-1].ts, span=Math.max(1,t1-t0);
  ctx.strokeStyle=color; ctx.lineWidth=2.5; ctx.beginPath();
  let pen=false;   // ⚡ v6.3: точки без значения (t:null из импорта) — разрыв линии, а не «0»
  data.forEach(r=>{ const v=r[key]; if(v==null){ pen=false; return; }
    const x=L+(r.ts-t0)/span*(w-L), y=10+(1-Math.min(v/maxY,1))*(h-20);
    pen?ctx.lineTo(x,y):ctx.moveTo(x,y); pen=true; });
  ctx.stroke();
}
function drawCharts(){
  const cut=Date.now()-rangeMs();
  const data=history.filter(r=>r.ts>=cut);
  drawLine(byId('chartP'),data,'p','#00d4ff',6.5);
  drawLine(byId('chartT'),data,'t','#ff8844',90);
  updateDbInfo();
}

/* ══════════ ⚡ v6.0: ЗВУК — НАСТРОЙКИ ══════════
   Вывод/громкость следуют за статусом (sout/svol), пока пользователь не начал
   правку (sndDirty). Остальное — из /snd/cfg при открытии вкладки. */
let sndCfg=null, sndDirty=false;
const sndUi={out:2,vol:10};
const SND_BUILTIN=[{id:-1,name:'H2 PRO',bi:1},{id:-2,name:'Сигнал включения',bi:1},{id:-3,name:'Тревога',bi:1},
  {id:-4,name:'Выключение',bi:1},{id:-5,name:'Предупреждение',bi:1},{id:-6,name:'Продувка завершена',bi:1}];
// ⚡ v6.5: «Где играть» применяется СРАЗУ (/snd/set?out= — только вывод, прочие
// несохранённые правки не трогаем): тревоги, события, щелчки, «Бумер» пульта и уже
// звучащая мелодия переходят на выбранное устройство без «Применить». Пока запрос в
// пути (sndOutPending), статус не перекрашивает выбор обратно; ошибка — откат.
let sndOutPending=0, sndOutSeq=0, sndOutOk=2;
function sndSetOut(v){
  if(v===sndUi.out&&!sndOutPending) return;
  const my=++sndOutSeq; sndUi.out=v; sndPaintOut(); sndOutPending++;
  getJson('/snd/set?out='+v)
    .then(()=>{ sndOutOk=v; if(sndCfg) sndCfg.out=v;
      if(my===sndOutSeq) flash('sndOk',v===3?'✓ звук выключен':'✓ звук: '+['пульт','мозг','оба'][v]); })
    .catch(e=>{ if(my===sndOutSeq){ sndUi.out=sndOutOk; sndPaintOut(); flash('sndOk','✗ '+netErr(e),true); } })
    .finally(()=>{ setTimeout(()=>sndOutPending--,800); });   // статус, отправленный до смены, не откатит выбор
}
function sndPaintOut(){ document.querySelectorAll('#sndOutSeg .seg-opt').forEach(b=>b.classList.toggle('active',+b.dataset.v===sndUi.out)); }
function sndOnVol(v){ sndUi.vol=+v; sndDirty=true; sndPaintVol(); }
function sndPaintVol(){ setRng(byId('sndVol'),sndUi.vol); txt('sndVolVal',sndUi.vol); }
// ⚡ v6.7: мелодии событий — [ключ /snd/cfg, id списка, встроенная по умолчанию, можно «Нет», предел своей, мс]
const SND_EVENTS=[['boot','sndBoot',-1,1,0],['on','sndOn',-2,1,0],['off','sndOff',-4,1,0],['pur','sndPur',-6,1,0],
  ['alr','sndAlr',-3,0,10000],['wrn','sndWrn',-5,0,10000]];
function sndLoadCfg(){
  return getJson('/snd/cfg').then(r=>r.json())
    .then(c=>{ sndCfg=c;
      if(!sndOutPending){ sndUi.out=sndOutOk=c.out; sndPaintOut(); }   // ⚡ v6.5: вывод — всегда с мозга
      if(!sndDirty){ sndUi.vol=c.vol; sndPaintVol();
        const a=byId('sndAlm'), k=byId('sndClk'); if(a) a.checked=!!c.alm; if(k) k.checked=!!c.clk; }
      SND_EVENTS.forEach(e=>sndFillSel(e,c[e[0]]));
      melRenderLib(); })
    .catch(()=>{ txt('melInfo','Нет ответа от мозга — список мелодий недоступен'); });
}
// Список «мелодия события»: Нет / встроенные / свои (для тревог — только короткие). Во время правки выбор не сбрасываем.
function sndFillSel(ev,val){
  const [,id,def,none,max]=ev, s=byId(id); if(!s) return;
  const want=(sndDirty&&s.value!=='')?s.value:String(val===undefined?def:val);
  s.innerHTML='';
  const add=(v,t)=>{ const o=document.createElement('option'); o.value=String(v); o.textContent=t; s.appendChild(o); };
  if(none) add(-9,'Нет');
  const bi=SND_BUILTIN.find(b=>b.id===def); add(def,(bi?bi.name:'Стандартная')+' (встроенная)');
  [-1,-2].filter(v=>v!==def).forEach(v=>add(v,SND_BUILTIN.find(b=>b.id===v).name+' (встроенная)'));
  ((sndCfg&&sndCfg.list)||[]).filter(m=>!max||m.ms<=max).forEach(m=>add(m.id,m.name));
  s.value=want; if(s.value!==want) s.value=String(def);
}
function sndApply(){
  if(!sndCfg){ flash('sndOk','✗ нет данных от мозга — повторите',true); sndLoadCfg(); return; }
  const q='out='+sndUi.out+'&vol='+sndUi.vol+'&alm='+(byId('sndAlm').checked?1:0)+'&clk='+(byId('sndClk').checked?1:0)
    +SND_EVENTS.map(e=>'&'+e[0]+'='+byId(e[1]).value).join('');
  getJson('/snd/set?'+q).then(()=>{ sndDirty=false; flash('sndOk','✓ настройки звука сохранены'); sndLoadCfg(); })
    .catch(e=>flash('sndOk','✗ '+netErr(e),true));
}
// Сыграть на устройстве — туда, что выбрано в «Где играть»
function sndPlayId(id,msgId){
  if(sndUi.out===3){ flash(msgId,'Звук выключен — выберите, где играть',true); return; }
  // ⚡ v6.5: без out — маршрут берётся из «Где играть» мозга на каждую ноту
  getJson('/snd/play?id='+id)
    .catch(e=>flash(msgId,'✗ '+(e.status===404?'мелодия не найдена':netErr(e)),true));
}
function sndTest(){ sndPlayId(-1,'sndOk'); }
function sndStop(){ getJson('/snd/stop').catch(e=>flash('sndOk','✗ '+netErr(e),true)); }

/* ══════════ ⚡ v6.0: ЗВУК — БИБЛИОТЕКА ══════════ */
const ICO={
  play:'<svg viewBox="0 0 24 24"><path d="M8 5.5v13l10.5-6.5z"/></svg>',
  ear:'<svg viewBox="0 0 24 24"><path d="M4 18v-5a8 8 0 0 1 16 0v5"/><path d="M4 15h3v6H5a1 1 0 0 1-1-1zM20 15h-3v6h2a1 1 0 0 0 1-1z"/></svg>',
  stop:'<svg viewBox="0 0 24 24"><rect x="6.5" y="6.5" width="11" height="11" rx="1.5"/></svg>',
  del:'<svg viewBox="0 0 24 24"><path d="M7 7l10 10M17 7L7 17"/></svg>',
  send:'<svg viewBox="0 0 24 24"><path d="M5 12h12M13 7l5 5-5 5"/></svg>'
};
function melFmtBytes(b){ return b>=1048576?(b/1048576).toFixed(2)+' МБ':Math.round(b/1024)+' КБ'; }
function melRowHtml(m){
  const meta=m.bi?'встроенная':(m.n+' нот · '+(m.ms/1000).toFixed(1)+' с');
  return `<div class="mel-row"><div class="mel-main"><div class="mel-name">${esc(m.name)}</div><div class="mel-meta">${meta}</div></div>`
    +`<button class="ico-btn" title="Сыграть на устройстве" aria-label="Сыграть мелодию на устройстве" onclick="melDevPlay(${m.id})">${ICO.play}</button>`
    +`<button class="ico-btn" data-pv="${m.id}" title="Прослушать в браузере (как на зуммере)" aria-label="Прослушать мелодию в браузере" onclick="melPreview(${m.id})">${ICO.ear}</button>`
    +(m.bi?'<span class="ico-ph"></span><span class="ico-ph"></span>':`<button class="ico-btn" title="Отправить в пульт" aria-label="Отправить мелодию в пульт" onclick="rmSendBrain(${m.id})">${ICO.send}</button>`
      +`<button class="ico-btn del" title="Удалить" aria-label="Удалить мелодию" onclick="melDel(${m.id})">${ICO.del}</button>`)
    +'</div>';
}
function melRenderLib(){
  const box=byId('melList'); if(!box) return;
  const c=sndCfg, user=((c&&c.list)||[]).slice().sort((a,b)=>a.id-b.id);
  let h='';
  if(c&&!c.fsok) h+='<div class="mel-empty">Память мелодий (LittleFS) недоступна — работают только встроенные</div>';
  else if(c&&!user.length) h+='<div class="mel-empty">Своих мелодий пока нет — добавьте ниже</div>';
  h+=user.map(melRowHtml).join('')+SND_BUILTIN.map(melRowHtml).join('');
  box.innerHTML=h;
  txt('melSlots',c?user.length+' из 16 слотов':'');
  txt('melInfo',c?(c.fsok?'Свободно '+melFmtBytes(c.free)+' из '+melFmtBytes(c.total):''):'');
  melPlUi();
}
function melDevPlay(id){ sndPlayId(id,'melOk'); }
function melPreview(id){
  if(melPl&&melPl.owner==='lib'+id){ melStopPreview(); return; }
  try{ melCtx(); }catch(e){ flash('melOk','✗ '+e.message,true); return; }   // AudioContext — строго в обработчике нажатия (iOS)
  getJson('/snd/get?id='+id).then(r=>r.arrayBuffer())
    .then(ab=>melPlay(melParseH2M(ab).notes,'lib'+id,false))
    .catch(e=>flash('melOk','✗ '+(e.status?'мелодия не найдена':netErr(e)),true));
}
function melDel(id){
  const m=((sndCfg&&sndCfg.list)||[]).find(x=>x.id===id);
  if(!confirm('Удалить мелодию «'+(m?m.name:id)+'»?')) return;
  if(melPl&&melPl.owner==='lib'+id) melStopPreview();
  getJson('/snd/del?id='+id).then(()=>{ flash('melOk','✓ удалено'); sndLoadCfg(); })
    .catch(e=>flash('melOk','✗ '+netErr(e),true));
}

/* ══════════ ⚡ v6.7: МЕЛОДИИ ПУЛЬТА (/rmel/*) ══════════
   Опись пульта мозг получает по радио; пока идёт передача — опрос раз в 0.5 с. */
let rmCfg=null, rmDirty=false, rmTimer=0, rmBusy=false;
function rmLoad(force){
  clearTimeout(rmTimer);
  return getJson('/rmel/cfg').then(r=>r.json()).then(c=>{ rmCfg=c; rmRender();
      const st=c.job&&c.job.st, active=st==='send'||st==='check';
      // опись ещё не пришла (пульт отвечает через мозг) или идёт передача — спросить ещё раз
      if(byId('page-sound').classList.contains('active')&&(active||(c.online&&!c.complete)||force===true&&!c.known))
        rmTimer=setTimeout(rmLoad,active?500:900);
    })
    .catch(()=>{ txt('rmSt','нет ответа'); cn('rmSt','pill bad'); });
}
function rmFillSel(id,val,first){
  const s=byId(id); if(!s) return;
  const want=(rmDirty&&s.value!=='')?s.value:String(val);
  s.innerHTML='';
  const add=(v,t)=>{ const o=document.createElement('option'); o.value=String(v); o.textContent=t; s.appendChild(o); };
  add(first[0],first[1]); ((rmCfg&&rmCfg.list)||[]).forEach(m=>add(m.id,m.name));
  s.value=want; if(s.value!==want) s.value=String(first[0]);
}
function rmRender(){
  const c=rmCfg; if(!c) return;
  setPill('rmSt',c.online,c.known?(c.list.length+' из '+c.slots):'на связи','нет связи');
  const j=c.job||{}, p=byId('rmProg');
  if(j.st==='send'||j.st==='check'){ p.classList.add('show'); byId('rmProgBar').style.width=(j.pct|0)+'%'; }
  else p.classList.remove('show');
  let h='';
  if(!c.known) h='<div class="mel-empty">'+(c.online?'Запрашиваю список у пульта…':'Пульт не на связи — включите его')+'</div>';
  else if(!c.fsok) h='<div class="mel-empty">Память пульта (LittleFS) недоступна</div>';
  else if(!c.list.length) h='<div class="mel-empty">В пульте только «Бумер» — добавьте свою мелодию ниже («В пульт»)</div>';
  else h=c.list.map(m=>`<div class="mel-row"><div class="mel-main"><div class="mel-name">${esc(m.name)}${m.id===c.act?' · ▶ кнопка':''}${m.id===c.boot?' · при включении':''}</div>`
    +`<div class="mel-meta">${m.ms?m.n+' нот · '+(m.ms/1000).toFixed(1)+' с':'…'}</div></div>`
    +`<span class="ico-ph"></span><span class="ico-ph"></span><button class="ico-btn del" title="Удалить из пульта" aria-label="Удалить мелодию из пульта" onclick="rmDel(${m.id})">${ICO.del}</button></div>`).join('');
  byId('rmList').innerHTML=h;
  if(c.known){ rmFillSel('rmAct',c.act,[-1,'«Бумер» (встроенный)']); rmFillSel('rmBoot',c.boot,[-9,'Нет']); }
}
function rmApply(){
  if(!rmCfg||!rmCfg.known){ flash('rmOk','✗ список пульта ещё не получен',true); rmLoad(true); return; }
  getJson('/rmel/set?act='+byId('rmAct').value+'&boot='+byId('rmBoot').value).then(r=>r.json()).then(j=>{
      if(!j.ok) throw new Error(j.err); rmDirty=false; flash('rmOk','✓ отправлено на пульт'); setTimeout(()=>rmLoad(true),600); })
    .catch(e=>flash('rmOk','✗ '+(e.message||netErr(e)),true));
}
function rmDel(id){
  const m=((rmCfg&&rmCfg.list)||[]).find(x=>x.id===id);
  if(!confirm('Удалить из пульта «'+(m?m.name:id)+'»?')) return;
  getJson('/rmel/del?id='+id).then(r=>r.json()).then(j=>{ if(!j.ok) throw new Error(j.err); flash('rmOk','✓ удалено'); setTimeout(()=>rmLoad(true),600); })
    .catch(e=>flash('rmOk','✗ '+(e.message||netErr(e)),true));
}
// Отправка мелодии из библиотеки мозга в пульт
function rmSendBrain(id){
  if(rmBusy) return;
  rmBusy=true;
  H2Mel.uploadRemote([{name:(((sndCfg&&sndCfg.list)||[]).find(x=>x.id===id)||{}).name||'мелодия',bytes:null}],
      ()=>getJson('/rmel/send?id='+id).then(r=>r.json()), rmCfgJson, t=>flash('melOk',t))
    .then(()=>{ flash('melOk','✓ мелодия в пульте — выберите её в блоке «Мелодии пульта»'); rmLoad(true); })
    .catch(e=>flash('melOk','✗ '+(e.message||netErr(e)),true))
    .finally(()=>{ rmBusy=false; });
}
function rmCfgJson(){ return getJson('/rmel/cfg').then(r=>r.json()).then(c=>{ rmCfg=c; rmRender(); return c; }); }

/* ══════════ ⚡ v6.0/v6.7: ПРОСЛУШИВАНИЕ В БРАУЗЕРЕ (WebAudio) ══════════
   ⚡ v6.7: «как на зуммере» — H2Mel.play: скважность громкости из «Звука», резонанс
   зуммера; music — огибающая кнопки МЕЛОДИЯ пульта. */
let melAc=null, melPl=null;
function melCtx(){
  if(!melAc){ const AC=window.AudioContext||window.webkitAudioContext;
    if(!AC) throw new Error('Браузер не поддерживает WebAudio'); melAc=new AC(); }
  if(melAc.state==='suspended') melAc.resume();
  return melAc;
}
function melPlay(notes,owner,music,onEnd){
  melStopPreview();
  const h=H2Mel.play(melCtx(),notes,{vol:sndUi.vol,music,onEnd:()=>{ if(melPl&&melPl.h===h){ melPl=null; melPlUi(); if(onEnd) onEnd(); } }});
  melPl={h,owner};
  melPlUi();
}
function melStopPreview(){
  if(!melPl) return;
  const p=melPl; melPl=null; p.h.stop();
  melPlUi();
}
function melPlUi(){
  const own=melPl?melPl.owner:'';
  txt('melListenBtn',own==='res'?'■ Стоп':'Прослушать');
  document.querySelectorAll('[data-pv]').forEach(b=>{ const on=own===(b.dataset.pv.charAt(0)==='p'?b.dataset.pv:'lib'+b.dataset.pv);
    if(b.classList.contains('on')!==on||!b.innerHTML){ b.classList.toggle('on',on); b.innerHTML=on?ICO.stop:ICO.ear; } });
}

/* ══════════ ⚡ v6.0: ФОРМАТ H2M1 (общий с прошивкой мозга) ══════════
   'H2M1' | nameLen u8 (≤48) | name UTF-8 | noteCount u16 | {freq u16, dur u16}×N, little-endian */
function melNameBytes(s){
  const enc=new TextEncoder(), parts=[]; let n=0;
  for(const ch of s.replace(/[\x00-\x1f\x7f]/g,'')){ const b=enc.encode(ch); if(n+b.length>48) break; parts.push(b); n+=b.length; }
  const out=new Uint8Array(n); let p=0; parts.forEach(b=>{ out.set(b,p); p+=b.length; });
  return out;
}
function melBuildH2M(name,notes){
  const nb=melNameBytes(name), u=new Uint8Array(4+1+nb.length+2+notes.length*4), dv=new DataView(u.buffer);
  u.set([72,50,77,49],0); u[4]=nb.length; u.set(nb,5);
  let p=5+nb.length; dv.setUint16(p,notes.length,true); p+=2;
  for(const n of notes){ dv.setUint16(p,n.f,true); dv.setUint16(p+2,n.d,true); p+=4; }
  return u;
}
function melParseH2M(ab){
  const u=new Uint8Array(ab), dv=new DataView(ab);
  if(u.length<7||u[0]!==72||u[1]!==50||u[2]!==77||u[3]!==49) throw new Error('неверный формат мелодии');
  let p=5+u[4]; if(u.length<p+2) throw new Error('файл мелодии повреждён');
  const name=new TextDecoder().decode(u.subarray(5,p)), cnt=dv.getUint16(p,true); p+=2;
  if(u.length<p+cnt*4) throw new Error('файл мелодии повреждён');
  const notes=[]; for(let i=0;i<cnt;i++,p+=4) notes.push({f:dv.getUint16(p,true),d:dv.getUint16(p+2,true)});
  return {name,notes};
}

/* ══════════ ⚡ v6.0: РАЗБОР ИСТОЧНИКОВ ══════════
   Все разборщики дают «сырую» монофоническую мелодию [{m,d}]:
   m — номер ноты MIDI (A4=69=440 Гц), m<0 — пауза; d — длительность, мс. */
function melReadFile(f,asText){
  return new Promise((res,rej)=>{ const r=new FileReader();
    r.onload=()=>res(r.result); r.onerror=()=>rej(new Error('Не удалось прочитать файл'));
    if(asText) r.readAsText(f); else r.readAsArrayBuffer(f); });
}
function melText(u){
  let s=''; try{ s=new TextDecoder('utf-8',{fatal:true}).decode(u); }
  catch(e){ try{ s=new TextDecoder('windows-1251').decode(u); }catch(e2){ s=''; } }
  return s.replace(/[\x00-\x1f]/g,'').trim();
}

// RTTTL: «имя:d=4,o=5,b=63:8c,8e.,p,c6» — ноты [длит][нота][#][.][октава][.]
function melParseRtttl(src){
  const lines=String(src).split(/\r?\n/).map(s=>s.trim()).filter(Boolean);
  const i=lines.findIndex(l=>(l.match(/:/g)||[]).length>=2);
  if(i<0) throw new Error('Не похоже на RTTTL: нужен формат «имя:d=4,o=5,b=120:ноты»');
  let s=lines[i]; for(let j=i+1;j<lines.length&&lines[j].indexOf(':')<0;j++) s+=lines[j];   // ноты, перенесённые на след. строки
  const c1=s.indexOf(':'), c2=s.indexOf(':',c1+1), def={d:4,o:6,b:63};
  s.slice(c1+1,c2).split(',').forEach(kv=>{ const m=kv.trim().toLowerCase().match(/^([dob])\s*=\s*(\d+)$/); if(m) def[m[1]]=+m[2]; });
  if(!(def.b>0)||!(def.d>0)) throw new Error('RTTTL: неверные параметры d= или b=');
  const q=60000/def.b, SEMI={c:0,d:2,e:4,f:5,g:7,a:9,b:11,h:11}, raw=[];
  s.slice(c2+1).split(',').forEach(tk=>{ tk=tk.trim().toLowerCase(); if(!tk) return;
    const m=tk.match(/^(\d{1,2})?([a-hp])(#)?(\.)?(\d)?(\.)?$/);
    if(!m) throw new Error('RTTTL: не удалось разобрать «'+tk+'»');
    const len=m[1]?+m[1]:def.d; if(!(len>0)) throw new Error('RTTTL: нулевая длительность в «'+tk+'»');
    let d=4*q/len; if(m[4]||m[6]) d*=1.5;
    if(m[2]==='p') raw.push({m:-1,d});
    else raw.push({m:12*((m[5]!==undefined?+m[5]:def.o)+1)+SEMI[m[2]]+(m[3]?1:0),d});
  });
  if(!raw.some(x=>x.m>=0)) throw new Error('RTTTL: нет нот');
  return {name:s.slice(0,c1).trim(),raw};
}

// Карта темпа MIDI: тик → мс (по всем meta 0x51 всех дорожек)
function melTempoMap(tempos,div){
  tempos.sort((a,b)=>a.tick-b.tick);
  const seg=[{tick:0,ms:0,us:500000}];
  for(const tp of tempos){ const L=seg[seg.length-1];
    if(tp.tick===L.tick) L.us=tp.us;
    else seg.push({tick:tp.tick,ms:L.ms+(tp.tick-L.tick)*L.us/div/1000,us:tp.us}); }
  return tick=>{ let lo=0,hi=seg.length-1;
    while(lo<hi){ const mid=(lo+hi+1)>>1; if(seg[mid].tick<=tick) lo=mid; else hi=mid-1; }
    const g=seg[lo]; return g.ms+(tick-g.tick)*g.us/div/1000; };
}
// Standard MIDI File 0/1: дорожки с нотами (канал 10 — ударные — исключён)
function melParseMidi(ab){
  const b=new Uint8Array(ab);
  let base=-1;   // MThd может быть внутри RIFF/RMID — ищем в начале файла
  for(let i=0;i+4<=Math.min(b.length,64);i++) if(b[i]===77&&b[i+1]===84&&b[i+2]===104&&b[i+3]===100){ base=i; break; }
  if(base<0||b.length<base+14) throw new Error('Это не MIDI-файл (нет заголовка MThd)');
  const u16=p=>(b[p]<<8)|b[p+1], u32=p=>((b[p]<<24)>>>0)+(b[p+1]<<16)+(b[p+2]<<8)+b[p+3];
  const fmt=u16(base+8), ntr=u16(base+10), div=u16(base+12);
  if(div&0x8000) throw new Error('MIDI с SMPTE-временем не поддерживается');
  if(fmt>1) throw new Error('MIDI формата 2 не поддерживается (нужен формат 0 или 1)');
  if(!div) throw new Error('Повреждённый MIDI-файл');
  const tracks=[], tempos=[];
  let p=base+8+u32(base+4), tn=0;
  while(p+8<=b.length&&tn<ntr){
    const id=String.fromCharCode(b[p],b[p+1],b[p+2],b[p+3]), end=Math.min(p+8+u32(p+4),b.length);
    p+=8;
    if(id!=='MTrk'){ p=end; continue; }
    tn++;
    const tr={num:tn,name:'',notes:[]}, open=new Map();
    let tick=0, rs=0;
    const vlq=()=>{ let v=0,c; do{ if(p>=end) return v; c=b[p++]; v=v*128+(c&127); }while(c&128); return v; };
    while(p<end){
      tick+=vlq(); if(p>=end) break;
      let s=b[p];
      if(s&128) p++; else if(rs) s=rs; else break;   // running status
      if(s===0xFF){ const ty=b[p++], l=vlq(), q=p; p+=l;
        if(ty===0x51&&l>=3) tempos.push({tick,us:(b[q]<<16)|(b[q+1]<<8)|b[q+2]});
        else if(ty===0x03&&!tr.name) tr.name=melText(b.subarray(q,Math.min(q+l,end)));
        else if(ty===0x2F) break; }
      else if(s===0xF0||s===0xF7){ p+=vlq(); }
      else if(s>0xF0){ p+=(s===0xF2?2:s===0xF3?1:0); }
      else{ rs=s;
        const hi=s&0xF0, ch=s&15, one=(hi===0xC0||hi===0xD0), d1=b[p], d2=one?0:b[p+1];
        p+=one?1:2;
        if(ch===9) continue;   // ударные
        const key=ch*128+d1;
        if(hi===0x90&&d2>0){ if(!open.has(key)) open.set(key,[]); open.get(key).push(tick); }
        else if(hi===0x80||hi===0x90){ const qq=open.get(key);
          if(qq&&qq.length){ const s0=qq.shift(); if(tick>s0) tr.notes.push({s:s0,e:tick,n:d1}); } } }
    }
    open.forEach((qq,key)=>qq.forEach(s0=>{ if(tick>s0) tr.notes.push({s:s0,e:tick,n:key&127}); }));   // незакрытые
    p=end;
    if(tr.notes.length) tracks.push(tr);
  }
  if(!tracks.length) throw new Error('В MIDI-файле нет нот (ударные не считаются)');
  let auto=0, bn=-1, ba=-1;   // «Авто»: больше всего нот, при равенстве — выше средняя высота
  tracks.forEach((t,i)=>{ const avg=t.notes.reduce((a,x)=>a+x.n,0)/t.notes.length;
    if(t.notes.length>bn||(t.notes.length===bn&&avg>ba)){ bn=t.notes.length; ba=avg; auto=i; } });
  return {tracks,auto,toMs:melTempoMap(tempos,div)};
}
// Полифония → монофония «по верхнему голосу» (skyline): в каждый момент начала нот
// берём самую высокую; предыдущая нота обрывается началом следующей.
function melMidiRaw(mid,sel){
  const src=sel==='all'?[].concat(...mid.tracks.map(t=>t.notes)):mid.tracks[sel==='auto'?mid.auto:+sel].notes;
  const a=src.slice().sort((x,y)=>x.s-y.s||y.n-x.n), top=[];
  for(const x of a){ const L=top[top.length-1]; if(!L||L.s!==x.s) top.push(x); }
  const raw=[]; let t=0;
  for(let i=0;i<top.length;i++){ const x=top[i], s=mid.toMs(x.s);
    let e=mid.toMs(x.e); if(i+1<top.length) e=Math.min(e,mid.toMs(top[i+1].s));
    if(s>t) raw.push({m:-1,d:s-t});
    if(e>s){ raw.push({m:x.n,d:e-s}); t=e; } else t=Math.max(t,s); }
  return raw;
}

/* ══════════ ⚡ v6.0: ПОДГОТОВКА ПОД ЗУММЕР ══════════ */
function melHz(m){ return 440*Math.pow(2,(m-69)/12); }
function melPrepare(raw,o){
  if(!raw.some(x=>x.m>=0&&x.d>0)) throw new Error('В источнике не найдено ни одной ноты');
  let k=0;
  if(o.oct==='auto'){ let best=-1;   // сдвиг, при котором больше всего звучания в 700–3500 Гц
    for(const kk of [0,1,-1,2,-2,3,-3,4,-4]){ let s=0;
      for(const x of raw) if(x.m>=0){ const f=melHz(x.m+12*kk); if(f>=700&&f<=3500) s+=x.d; }
      if(s>best){ best=s; k=kk; } } }
  else k=o.oct;
  const sc=100/o.tempo;
  // 1) сдвиг октав, сворачивание в 200–5000 Гц, темп; соседние паузы — вместе
  const a=[];
  for(const x of raw){ if(!(x.d>0)) continue;
    let m=-1; if(x.m>=0){ m=x.m+12*k; while(melHz(m)<200) m+=12; while(melHz(m)>5000) m-=12; }
    const Lp=a[a.length-1]; if(m<0&&Lp&&Lp.m<0) Lp.d+=x.d*sc; else a.push({m,d:x.d*sc}); }
  // 2) ноты короче 25 мс и паузы короче 10 мс — в предыдущий элемент
  const b=[];
  for(const x of a){ const Lp=b[b.length-1];
    if(Lp&&((x.m>=0&&x.d<25)||(x.m<0&&(x.d<10||Lp.m<0)))) Lp.d+=x.d; else b.push(x); }
  // 3) артикуляция: между соседними нотами без паузы — короткая пауза
  const c=[];
  for(let i=0;i<b.length;i++){ const x=b[i], nx=b[i+1];
    if(o.gap>0&&x.m>=0&&nx&&nx.m>=0){ const g=Math.min(o.gap,x.d*0.25); c.push({m:x.m,d:x.d-g},{m:-1,d:g}); }
    else c.push(x); }
  // 4) паузы < 10 мс убрать (время — предыдущей ноте), соседние паузы объединить
  const e=[];
  for(const x of c){ const Lp=e[e.length-1];
    if(x.m<0&&Lp&&Lp.m<0){ Lp.d+=x.d; continue; }
    if(x.m<0&&x.d<10){ if(Lp) Lp.d+=x.d; continue; }
    e.push(x); }
  // 5) паузы по краям — прочь; 6) звуки длиннее 10 с — на части
  while(e.length&&e[0].m<0) e.shift();
  while(e.length&&e[e.length-1].m<0) e.pop();
  const f=[];
  for(const x of e){ const parts=Math.ceil(x.d/10000); for(let i=0;i<parts;i++) f.push({m:x.m,d:x.d/parts}); }
  // 7) ограничения: ≤ 1200 нот и ≤ макс. длительности
  const g=[]; let tot=0, cutT=false, cutN=false;
  for(const x of f){
    if(g.length>=1200){ cutN=true; break; }
    if(tot+x.d>o.maxMs){ const rest=o.maxMs-tot; if(x.m>=0&&rest>=25) g.push({m:x.m,d:rest}); cutT=true; break; }
    g.push(x); tot+=x.d; }
  while(g.length&&g[g.length-1].m<0) g.pop();
  // 8) целые мс (накопленное округление — без дрейфа темпа), частоты — равномерная темперация
  const notes=[]; let acc=0, prev=0, fmin=1e9, fmax=0;
  for(const x of g){ acc+=x.d; let d=Math.round(acc)-prev; d=Math.max(1,Math.min(10000,d)); prev+=d;
    const fr=x.m<0?0:Math.round(melHz(x.m));
    if(fr){ fmin=Math.min(fmin,fr); fmax=Math.max(fmax,fr); }
    notes.push({f:fr,d}); }
  if(!fmax) throw new Error('После подготовки не осталось нот');
  return {notes,k,auto:o.oct==='auto',ms:prev,fmin,fmax,cutT,cutN,maxMs:o.maxMs};
}

/* ══════════ ⚡ v6.0/v6.7: ИМПОРТ — ИНТЕРФЕЙС ══════════
   Поток: файл / RTTTL-текст → (отрезки) → «Подготовить» → «Прослушать» → «Сохранить в мозг» / «В пульт».
   ⚡ v6.7: шкала трека с отрезками (H2Mel.SegEditor): склеить в одну мелодию с паузами
   или каждый отрезок отдельно. Аудио распознаётся только на выбранных отрезках.
   После первой подготовки смена параметров пересчитывает результат сразу. */
const MEL_FILE_HINT='MIDI, RTTTL или аудио (mp3, wav, ogg, m4a)';
let melSrc=null, melMidi=null, melAudio=null, melRaw=null, melLastPieces=null, melRes=null, melBusy=false, melNameAuto=true, melSaving=false;
let melSeg=null;
function melSegInit(){
  if(melSeg) return;
  melSeg=new H2Mel.SegEditor(byId('melSeg'),{getCtx:()=>{ try{ return melCtx(); }catch(e){ return null; } },onChange:melOnSeg});
}
function melMsgSet(id,t,c){ const e=byId(id); if(!e) return; e.textContent=t; e.className='msg'+(c?' '+c:''); }
function melProg(fr,label){
  const p=byId('melProg'); if(!p) return;
  if(fr<0){ p.classList.remove('show'); return; }
  p.classList.add('show'); byId('melProgBar').style.width=Math.round(fr*100)+'%';
  melMsgSet('melMsg',label+(fr>0?' '+Math.round(fr*100)+' %':''),'');
}
function melSetName(n){ if(melNameAuto){ const e=byId('melName'); if(e) e.value=n||'Мелодия'; } }
function melOpts(){ const o=byId('melOct').value;
  return {oct:o==='auto'?'auto':+o,tempo:+byId('melTempo').value,gap:+byId('melGap').value,maxMs:+byId('melMax').value*1000}; }
function melClearRes(){
  if(melPl&&(melPl.owner==='res'||melPl.owner.charAt(0)==='p')) melStopPreview();
  melRes=null; const r=byId('melRes'); if(r) r.classList.remove('show'); melMsgSet('melSaveMsg','','');
}
function melShowSeg(src){ melSegInit(); melSeg.setSource(src); byId('melSegLbl').style.display=src?'':'none'; }
function melNewSource(){ melClearRes(); melLastPieces=null; melMidi=null; melAudio=null; melRaw=null; melNameAuto=true;
  byId('melTrkFld').style.display='none'; melMsgSet('melMsg','',''); melShowSeg(null); }
async function melOnFile(inp){
  const f=inp.files[0]; inp.value=''; if(!f) return;
  melNewSource(); melSrc=null; byId('melText').value='';
  const ext=(f.name.split('.').pop()||'').toLowerCase();
  const kind=['mid','midi','kar','rmi'].includes(ext)?'midi'
    :['rtttl','rtx','txt'].includes(ext)?'rtttl'
    :(['mp3','wav','ogg','oga','opus','m4a','aac','flac','webm'].includes(ext)||/^audio\//.test(f.type))?'audio':null;
  txt('melFileName',f.name);
  if(!kind){ txt('melFileName',MEL_FILE_HINT); melMsgSet('melMsg','Формат не поддерживается: нужен .mid, .rtttl/.txt или аудиофайл','bad'); return; }
  const src={kind,file:f}; melSrc=src; melSetName(f.name.replace(/\.[^.]*$/,''));
  try{
    if(kind==='midi'){
      melMidi=melParseMidi(await melReadFile(f,false));
      const s=byId('melTrk'); s.innerHTML='';
      const add=(v,t)=>{ const o=document.createElement('option'); o.value=String(v); o.textContent=t; s.appendChild(o); };
      const lbl=t=>t.num+': '+(t.name||'Дорожка')+' ('+t.notes.length+' нот)';
      add('auto','Авто — '+lbl(melMidi.tracks[melMidi.auto])); add('all','Все дорожки (верхний голос)');
      melMidi.tracks.forEach((t,i)=>add(i,lbl(t)));
      byId('melTrkFld').style.display='';
      melRaw=melMidiRaw(melMidi,'auto'); melShowSeg({kind:'notes',raw:melRaw});
    } else if(kind==='rtttl'){
      const r=melParseRtttl(await melReadFile(f,true)); if(r.name) melSetName(r.name);
      melRaw=r.raw; melShowSeg({kind:'notes',raw:melRaw});
    } else {
      melMsgSet('melMsg','Декодирование аудио…','');
      const ab=await melReadFile(f,false), buf=await H2Mel.decode(melCtx(),ab);
      if(src!==melSrc) return;   // файл сменили во время декодирования
      melAudio={file:f,buffer:buf,cache:new Map()};
      melShowSeg({kind:'audio',buffer:buf});
      melMsgSet('melMsg','Длительность '+(buf.duration).toFixed(1)+' с. Выделите отрезки на шкале или нажмите «Подготовить» — возьмётся начало трека.','');
    }
  }catch(e){ if(src!==melSrc) return; melSrc=null; melShowSeg(null); melMsgSet('melMsg',(e&&e.message)||String(e),'bad'); }
}
let melTextT=0;
function melOnText(){
  const has=byId('melText').value.trim()!=='';
  if(!melSrc||melSrc.kind!=='text'){ melNewSource(); txt('melFileName',MEL_FILE_HINT); }
  else melClearRes();
  melSrc=has?{kind:'text'}:null;
  clearTimeout(melTextT);
  melTextT=setTimeout(()=>{ if(!melSrc||melSrc.kind!=='text') return;
    try{ const r=melParseRtttl(byId('melText').value); melSetName(r.name); melRaw=r.raw;
      if(melSeg&&melSeg.src&&melSeg.src.kind==='notes') melSeg.updateNotes(melRaw); else melShowSeg({kind:'notes',raw:melRaw}); }
    catch(e){ melRaw=null; melShowSeg(null); } },400);
}
function melSegs(){ return melSeg&&melSeg.src?melSeg.segs.map(s=>({a:Math.round(s.a),b:Math.round(s.b),gap:s.gap|0})):[]; }
// «Сырые» мелодии [{m,d}] по отрезкам: одна (склеить / без отрезков) или несколько
async function melGetPieces(){
  if(!melSrc) throw new Error('Выберите файл или вставьте RTTTL-текст');
  const segs=melSegs(), join=!melSeg||melSeg.join;
  if(melSrc.kind!=='audio'){
    if(melSrc.kind==='text'){ const r=melParseRtttl(byId('melText').value); melSetName(r.name); melRaw=r.raw; }
    if(!melRaw) throw new Error('В источнике не найдено нот');
    return H2Mel.pieces(segs,join,s=>H2Mel.slice(melRaw,s.a,s.b),()=>melRaw);
  }
  // аудио: распознаём только выбранные отрезки (результат кэшируется по границам)
  const A=melAudio; if(!A) throw new Error('Аудио ещё не декодировано');
  const parts=segs.length?segs:[{a:0,b:Math.min(A.buffer.duration*1000,+byId('melSecs').value*1000),gap:0}];
  const tot=parts.reduce((s,x)=>s+x.b-x.a,0);
  if(tot>180000) throw new Error('Отрезки длиннее 3 минут в сумме — распознавание будет слишком долгим, сократите их');
  let done=0; const raws=[];
  for(const p of parts){ const key=p.a+'-'+p.b;
    if(!A.cache.has(key)) A.cache.set(key,await H2Mel.pitchTrack(A.buffer,p.a,p.b,fr=>melProg((done+fr*(p.b-p.a))/tot,'Анализ аудио…')));
    done+=p.b-p.a; raws.push(A.cache.get(key)); }
  if(!raws.some(r=>r.some(x=>x.m>=0))) throw new Error('В аудио не найдено устойчивых нот — лучше всего подходят простые одноголосые рингтоны');
  let i=0;
  return H2Mel.pieces(segs,join,()=>raws[i++],()=>raws[0]);
}
function melRunPrep(pieces){
  if(melPl&&(melPl.owner==='res'||melPl.owner.charAt(0)==='p')) melStopPreview();
  const o=melOpts(), list=[], skipped=[];
  pieces.forEach((p,i)=>{ try{ list.push(melPrepare(p,o)); }catch(e){ skipped.push(i+1); } });
  if(!list.length) throw new Error('После подготовки не осталось нот');
  melRes={list,skipped}; melLastPieces=pieces;
  const r=list[0], ms=list.reduce((s,x)=>s+x.ms,0), n=list.reduce((s,x)=>s+x.notes.length,0);
  const fmin=Math.min(...list.map(x=>x.fmin)), fmax=Math.max(...list.map(x=>x.fmax));
  const ex=['сдвиг октав '+(r.k>0?'+':'')+String(r.k).replace('-','−')+(r.auto?' (авто)':'')];
  if(list.some(x=>x.cutT)) ex.push('обрезано до '+(r.maxMs/1000)+' с');
  if(list.some(x=>x.cutN)) ex.push('обрезано до 1200 нот');
  if(skipped.length) ex.push('без нот, пропущены отрезки: '+skipped.join(', '));
  txt('melStat',(list.length>1?list.length+' мелодии · ':'')+n+' нот · '+(ms/1000).toFixed(1)+' с · '+fmin+'–'+fmax+' Гц');
  txt('melStat2',ex.join(' · '));
  byId('melPieces').innerHTML=list.length>1?list.map((x,i)=>`<div class="mel-row"><div class="mel-main"><div class="mel-name">${i+1}. ${esc(melPieceName(i))}</div>`
    +`<div class="mel-meta">${x.notes.length} нот · ${(x.ms/1000).toFixed(1)} с</div></div><span class="ico-ph"></span>`
    +`<button class="ico-btn" data-pv="p${i}" aria-label="Прослушать мелодию ${i+1}" onclick="melListenPiece(${i})">${ICO.ear}</button><span class="ico-ph"></span></div>`).join(''):'';
  byId('melRes').classList.add('show'); melMsgSet('melSaveMsg','',''); melPlUi();
}
function melPieceName(i){ const b=(byId('melName').value.trim()||'Мелодия'); return melRes&&melRes.list.length>1?b+' '+(i+1):b; }
async function melPrepareClick(){
  if(melBusy) return;
  const btn=byId('melPrepBtn'), src=melSrc; melBusy=true; btn.disabled=true; melMsgSet('melMsg','','');
  try{ melCtx();   // AudioContext — в обработчике нажатия (iOS)
    const pieces=await melGetPieces();
    // ⚡ v6.3: файл/текст сменили во время анализа — результат устарел (иначе мелодия A сохранится под именем B)
    if(src!==melSrc){ melMsgSet('melMsg','Источник изменён — нажмите «Подготовить».',''); return; }
    melRunPrep(pieces); melMsgSet('melMsg','Готово — прослушайте и сохраните.','ok'); }
  catch(e){ if(src!==melSrc) return; melClearRes(); melMsgSet('melMsg',(e&&e.message)||String(e),'bad'); }
  finally{ melBusy=false; btn.disabled=false; melProg(-1); }
}
function melReprep(){
  if(!melLastPieces) return;
  try{ melRunPrep(melLastPieces); }catch(e){ melClearRes(); melMsgSet('melMsg',e.message,'bad'); }
}
// Отрезки изменились: ноты — пересчёт сразу; аудио — нужен новый анализ
function melOnSeg(){
  if(!melRes||melBusy) return;
  if(melSrc&&melSrc.kind==='audio'){ melClearRes(); melLastPieces=null; melMsgSet('melMsg','Отрезки изменены — нажмите «Подготовить».',''); return; }
  melGetPieces().then(melRunPrep).catch(e=>{ melClearRes(); melMsgSet('melMsg',e.message,'bad'); });
}
function melParamLabels(){
  const t=byId('melTempo'), g=byId('melGap'), m=byId('melMax'), s=byId('melSecs');
  setRng(t,t.value); setRng(g,g.value); setRng(m,m.value); setRng(s,s.value);
  txt('melTempoV',t.value+' %'); txt('melGapV',g.value+' мс'); txt('melMaxV',m.value+' с'); txt('melSecsV',s.value+' с');
}
function melOnParam(){
  melParamLabels();
  if(melRes&&!melBusy) melReprep();
}
function melOnSecs(){
  melParamLabels();
  if(melSrc&&melSrc.kind==='audio'&&!melSegs().length&&melRes&&!melBusy){ melClearRes(); melLastPieces=null;
    melMsgSet('melMsg','Длительность анализа изменена — нажмите «Подготовить».',''); }
}
function melOnTrack(){
  if(!melMidi) return;
  melRaw=melMidiRaw(melMidi,byId('melTrk').value); melSeg.updateNotes(melRaw);
}
function melPvMusic(){ return byId('melPvAs').value==='remote'; }
function melOnPvAs(){ if(melPl&&(melPl.owner==='res'||melPl.owner.charAt(0)==='p')) melListen(); }
// Прослушать всё подряд (несколько мелодий — одна за другой)
function melListen(){
  if(melPl&&melPl.owner==='res'){ melStopPreview(); return; }
  if(!melRes) return;
  try{ const all=[]; melRes.list.forEach((x,i)=>{ if(i) all.push({f:0,d:600}); all.push(...x.notes); });
    melPlay(all,'res',melPvMusic()); }catch(e){ melMsgSet('melSaveMsg',e.message,'bad'); }
}
function melListenPiece(i){
  if(melPl&&melPl.owner==='p'+i){ melStopPreview(); return; }
  try{ melPlay(melRes.list[i].notes,'p'+i,melPvMusic()); }catch(e){ melMsgSet('melSaveMsg',e.message,'bad'); }
}
function melFiles(){ return melRes.list.map((x,i)=>({name:melPieceName(i),bytes:melBuildH2M(melPieceName(i),x.notes)})); }
async function melSave(){
  if(!melRes||melSaving) return;
  const files=melFiles();
  if(sndCfg&&!sndCfg.fsok){ melMsgSet('melSaveMsg','✗ Память мелодий (LittleFS) недоступна','bad'); return; }
  if(sndCfg&&(sndCfg.list||[]).length+files.length>16){ melMsgSet('melSaveMsg','✗ Не хватит слотов мозга (16) — удалите ненужные мелодии','bad'); return; }
  const btn=byId('melSaveBtn'); btn.disabled=true; melSaving=true;
  let ok=0;
  try{
    for(const f of files){
      melMsgSet('melSaveMsg','Загрузка «'+f.name+'»…','');
      const fd=new FormData(); fd.append('f',new Blob([f.bytes],{type:'application/octet-stream'}),'melody.h2m');
      const r=await fetch('/snd/up',{method:'POST',body:fd}), t=await r.text();
      let j=null; try{ j=JSON.parse(t); }catch(e){}
      if(!(j&&j.ok)) throw new Error((j&&j.err)||t||('ошибка '+r.status));
      ok++;
    }
    melMsgSet('melSaveMsg','✓ Сохранено в мозг: '+ok+'. Мелодию можно выбрать для события выше.','ok');
  }catch(e){ melMsgSet('melSaveMsg','✗ '+(ok?'сохранено '+ok+', затем ошибка: ':'')+(e.message||netErr(e)),'bad'); }
  finally{ btn.disabled=false; melSaving=false; sndLoadCfg(); }
}
async function melSaveRemote(){
  if(!melRes||melSaving) return;
  const files=melFiles(), btn=byId('melRmBtn'); btn.disabled=true; melSaving=true;
  try{
    const n=await H2Mel.uploadRemote(files,b=>{ const fd=new FormData(); fd.append('f',new Blob([b],{type:'application/octet-stream'}),'melody.h2m');
        return fetch('/rmel/up',{method:'POST',body:fd}).then(r=>r.text()).then(t=>{ try{ return JSON.parse(t); }catch(e){ return {ok:0,err:t}; } }); },
      rmCfgJson,t=>melMsgSet('melSaveMsg',t,''));
    melMsgSet('melSaveMsg','✓ В пульт загружено: '+n+'. Выберите её для кнопки МЕЛОДИЯ в блоке «Мелодии пульта».','ok');
  }catch(e){ melMsgSet('melSaveMsg','✗ '+(e.message||netErr(e)),'bad'); }
  finally{ btn.disabled=false; melSaving=false; rmLoad(true); }
}

/* ══════════ РЕНДЕР ══════════
   ⚡ v6.3: все записи — через txt/cn/sty: неизменившееся в DOM не трогается. */
function setPill(id,ok,a,b){ cn(id,'pill '+(ok?'ok':'bad')); txt(id,ok?a:b); }
// Значки состояния: [id, ключ статуса, класс при активном]
const BADGES=[['bPwr','pwr','ok'],['bLck','lck','warn'],['bTlt','tlk','tilt'],['bPrg','prg','danger'],['bPsh','psh','warn'],
  ['bRep','rep','purple'],['bMgs','mgs','ok'],['bPzo','pzo','warn'],['bEle','ele','ok'],['bVal','val','ok']];
// Кнопки управления: [id, ключ статуса, базовый класс, добавка при активном]
const CTRLS=[['cLck','lck','ctrl wide',' active-warn'],['cRep','rep','ctrl repulsor-btn',' active-purple'],['cMgs','mgs','ctrl',' active'],
  ['cPzo','pzo','ctrl active-warn wide',' active-red'],['cPsh','psh','ctrl purge-btn wide',' active-red']];
// Тревога с наивысшим приоритетом: [доп. класс, текст] или null
function alertOf(d,p){
  if(d.tlk)  return ['tilt','⚠ НАКЛОН '+d.ta+'° — ЭЛЕКТРОЛИЗЕР И КЛАПАН ОБЕСТОЧЕНЫ'];
  if(d.ovl)  return ['danger','⚠ АВАРИЯ '+p.toFixed(2)+' БАР'];
  if(d.psf)  return ['danger','⚠ ДАТЧИК ДАВЛЕНИЯ НЕИСПРАВЕН ('+(+d.pv).toFixed(2)+' В) — ЭЛЕКТРОЛИЗЁР ЗАБЛОКИРОВАН'];
  if(d.mpuf) return ['tilt','⚠ ДАТЧИК НАКЛОНА НЕ ОТВЕЧАЕТ — ЭЛЕКТРОЛИЗЁР ЗАБЛОКИРОВАН'];   // ⚡ v6.3
  if(d.lkl)  return ['danger','⚠ УТЕЧКА — СИСТЕМА ВЫКЛЮЧЕНА. Проверьте клапан и магистраль, затем LOCK'];
  if(d.oh)   return ['danger','🔥 ПЕРЕГРЕВ '+(+d.t).toFixed(0)+'°C'];
  if(d.ovw)  return ['','⚠ Приближение к аварии: '+p.toFixed(2)+' / 7.2 бар'];
  if(d.wd)   return ['','⚠ БАК НЕ НАБИРАЕТ ДАВЛЕНИЕ'];
  if(d.lkw)  return ['','⚠ ДАВЛЕНИЕ ПАДАЕТ ПРИ ЗАКРЫТОМ КЛАПАНЕ — возможна утечка'];
  if(d.mntd) return ['','⚙ ПОРА ОБСЛУЖИТЬ: долить воду/электролит (Настройки → Обслуживание)'];
  if(d.relw) return ['','⚙ РЕСУРС РЕЛЕ НА ИСХОДЕ — проверьте контакты'];
  return null;
}

function render(){
  const d=st; if(d.p===undefined) return;
  const p=+d.p, now=Date.now();
  txt('pv',p.toFixed(2));
  sty('pf','width',Math.max(0,Math.min(p/6.5*100,100)).toFixed(1)+'%');
  sty('ptgt','left','calc('+Math.max(0,Math.min(d.tgt/6.5*100,100)).toFixed(1)+'% - 1px)');
  const lvl=d.ovl?' danger':d.ovw?' warn':'';
  cn('pBox','pressure'+lvl); cn('pNum','p-num'+lvl); cn('pf','p-fill'+lvl);

  // Температура и выводной вентилятор на реле.
  const tS=(d.tok?(+d.t).toFixed(1):'--')+'°';
  txt('tempVal',tS); cn('tempVal','temp-val'+(d.oh?' hot':''));
  fanState(2,!!d.f2);
  fanSync(2,d);

  // Tilt (⚡ v6.3: + MPU перестал отвечать / не найден — угла нет)
  { let tl='НАКЛОН (порог 30°)', tv=d.ta+'°', tc='';
    if(d.tcal){ tl='КАЛИБРОВКА MPU6050...'; tv='wait'; }
    else if(d.tlk){ tc=' danger'; tl='НАКЛОН — АВАРИЯ (LOCK)'; if(d.mpuf) tv='?'; }
    else if(d.mpuf){ tc=' danger'; tl='НАКЛОН — MPU6050 НЕ ОТВЕЧАЕТ'; tv='?'; }
    else if(!d.mpuok){ tc=' warn'; tl='НАКЛОН — ДАТЧИК НЕ НАЙДЕН'; tv='--°'; }
    else if(d.ta>=20) tc=' warn';
    cn('tiltRow','tilt-row'+tc); cn('tiltVal','tilt-val'+tc); txt('tiltLbl',tl); txt('tiltVal',tv); }

  // Алерты (приоритет); текст пишется только при смене — экранный диктор не повторяет
  { const a=alertOf(d,p);
    cn('alertBar','alert-bar'+(a?' show'+(a[0]?' '+a[0]:''):'')); txt('alertBar',a?a[1]:''); }
  // ⚡ v6.2: «заглушить» — пока есть тревога и повторы не заглушены
  sty('btnMute','display',((d.ovl||d.tlk||d.oh||d.psf||d.lkl||d.mpuf)&&!d.mute)?'':'none');
  // ⚡ v6.2: таймер работы
  txt('tmrLn', d.tml>=0?'⏱ '+fmtMS(d.tml):'');
  if(d.tml>=0){ cn('tmrSt','pill ok'); txt('tmrSt','осталось '+fmtMS(d.tml)); }
  else{ cn('tmrSt','pill'); txt('tmrSt',d.tmr?d.tmr+' мин':'без ограничения'); }
  if(d.tmr!==undefined) tmrSync(d.tmr);
  // ⚡ v6.2: обслуживание
  cn('mntSt','pill'+(d.mntd?' bad':' ok')); txt('mntSt',d.mntd?'ПОРА':'через '+d.mnt+' ч');
  if(!mntDirty&&d.mnti!==undefined) fld('mntI',d.mnti);
  // ⚡ v6.2: газ
  txt('gRate',(+d.grate||0).toFixed(1)); txt('gDuty',Math.round(+d.duty||0)+'%');
  txt('gSes',fmtNL(+d.gses||0)); txt('gTot',fmtNL(+d.gtot||0));
  txt('gDay',d.tsync?fmtNL(+d.gday):'—'); txt('gWk',d.tsync?fmtNL(+d.gwk):'—');
  if(!gasDirty&&d.gtank!==undefined) fld('gTank',(+d.gtank).toFixed(1));
  // ⚡ v6.3: мозг перезагрузился или первая синхронизация не прошла — повторить
  if(d.tsync===0&&now-lastSyncTry>15000) syncTime();
  // ⚡ v6.2: новые записи журнала — обновить список, если открыта диагностика
  if(d.evs!==undefined&&d.evs!==logSeen){ logSeen=d.evs;
    if(byId('page-diag').classList.contains('active')) logLoad(); }

  // Бейджи и кнопки
  BADGES.forEach(b=>cn(b[0],'badge'+(d[b[1]]?' '+b[2]:'')));
  cn('btnPwr','btn-power'+(d.pwr?' on':''));
  txt('btnPwr',d.pwr?'ВЫКЛЮЧИТЬ  (2× = ПРОДУВКА)':'ВКЛЮЧИТЬ СИСТЕМУ');
  const dis=!d.pwr||d.prg||d.lck||d.tlk;
  CTRLS.forEach(c=>{ cn(c[0],c[2]+(d[c[1]]?c[3]:''));
    if(c[0]!=='cLck'){ const e=byId(c[0]); if(e.disabled!==!!dis) e.disabled=!!dis; } });

  // Целевое — ⚡ v6.3: не трогаем, пока ползунок держат или цель правили < 20 с назад
  if(document.activeElement!==byId('tslider')&&(!tgtEdit||now-tgtEdit>20000)){
    tgtEdit=0; const v=+d.tgt; if(v!==pendingTgt){ pendingTgt=v; tgtPaint(v); } }

  // RF / uptime
  cn('rfDot','rf-dot'+(d.rf?' ok':' fail'));
  txt('rfTxt','ПУЛЬТ: '+(d.rf?'ОНЛАЙН':'ОФФЛАЙН'));
  txt('uptime','UP '+fmtUp(d.up));

  // OTA — ⚡ v6.3: мозг отклоняет и во время продувки
  { const ota=byId('otaLink'), no=d.pwr?'⬆ OTA — выключите систему':(d.prg||d.psh)?'⬆ OTA — идёт продувка':'';
    cn('otaLink','ota-link'+(no?' disabled':''));
    if(no){ if(ota.hasAttribute('href')) ota.removeAttribute('href'); }
    else if(!ota.hasAttribute('href')) ota.setAttribute('href','/update');
    txt('otaLink',no||'⬆ OTA-обновление прошивки'); }

  purging=!!d.prg;
  txt('foot',d.prg?'⏳ ПРОДУВКА ДО НУЛЯ — '+p.toFixed(2)+' бар'
    :d.psh?'⚡ МИНИ-ПРОДУВКА 200 мс'
    :d.rep?'🔥 РЕПУЛЬСОР — ВЫСТРЕЛ'
    :d.tlk?'⚠ TILT — выровняйте и нажмите LOCK'
    :d.ovl?'⚠ АВАРИЯ — СБРОСЬТЕ ДАВЛЕНИЕ'
    :(d.tme&&!d.pwr)?'⏱ Система выключена таймером':'');

  // Статистика
  txt('sPnow',p.toFixed(2)); txt('sTnow',tS);
  txt('sRt',Math.floor(d.rt/3600)+'ч '+Math.floor((d.rt%3600)/60)+'м');
  txt('sUp',fmtUp(d.up));
  [['sRce','rce'],['sRcv','rcv'],['sRcp','rcp'],['sRcf2','rcf2']].forEach(k=>txt(k[0],(d[k[1]]||0).toLocaleString()));
  txt('sRfh','к'+(d.rfc||76)+' / '+(d.rfh||0));
  txt('calCur',p.toFixed(2)); txt('calPv',(+d.pv||0).toFixed(3));
  calSync(d);
  // максимум — по истории (ведётся при записи) и текущему значению
  txt('sPmax',Math.max(histPMax,p).toFixed(2));
  { const tm=Math.max(histTMax,d.tok?+d.t:-Infinity); txt('sTmax',isFinite(tm)?tm.toFixed(1)+'°':'--°'); }

  // ⚡ v6.0: звук — вывод/громкость (пока не редактируем) и индикатор «играет»
  // ⚡ v6.5: вывод применяется сразу — следует статусу всегда, кроме запроса в пути
  if(d.sout!==undefined){
    if(!sndOutPending&&sndUi.out!==d.sout){ sndUi.out=sndOutOk=d.sout; sndPaintOut(); }
    if(!sndDirty&&sndUi.vol!==d.svol){ sndUi.vol=d.svol||10; sndPaintVol(); } }
  sty('sndPlay','visibility',d.sp?'visible':'hidden');

  // ── Диагностика модулей ──
  setPill('dgOled',d.oledok,d.lcdok?'LCD2004':'OLED','НЕ НАЙДЕН');
  setPill('dgMpu',d.mpuok&&!d.mpuf,'OK',d.mpuok?'НЕ ОТВЕЧАЕТ':'НЕ НАЙДЕН');
  setPill('dgNrf',d.nrfok,'OK','НЕ НАЙДЕН');
  setPill('dgPs',!d.psf,'OK','НЕИСПРАВЕН');
  txt('dgPv',(+d.pv||0).toFixed(2)+' В');
  setPill('dgTemp',d.tok,'OK','НЕТ ДАННЫХ');
  setPill('dgRf',d.rf,'ОНЛАЙН','ОФФЛАЙН');
  setPill('dgFs',d.fsok,'OK','НЕДОСТУПНА');
  { const hints=[];
    if(!d.oledok && !d.mpuok) hints.push('Не работают дисплей и MPU на общей шине I²C. Проверьте SDA→21, SCL→22, общую землю и уровни напряжения.');
    else { if(!d.oledok) hints.push('Дисплей не найден: LCD2004 проверяется по адресам 0x27/0x3F, OLED — 0x3C. Проверьте SDA→21, SCL→22, питание и общую землю.');
           if(!d.mpuok)  hints.push('MPU-6050 не найден: та же шина I²C (21/22), AD0→GND (адрес 0x68), VCC→3.3В.'); }
    if(d.mpuf)   hints.push('MPU-6050 перестал отвечать в работе: угол наклона неизвестен, электролизёр (MOSFET) заблокирован до восстановления связи. Проверьте провода SDA→21, SCL→22, питание 3.3 В и общую землю.');
    if(!d.nrfok) hints.push('NRF24 не найден: SCK→18, MISO→19, MOSI→23, CE→4, CSN→5; VCC→3.3В (НЕ 5В!), конденсатор 470мкФ у модуля.');
    if(d.psf)    hints.push('Датчик давления вне диапазона 0,15–3,2 В: обрыв, замыкание или нет питания датчика. Проверьте делитель и провод к GPIO 36. Электролизёр заблокирован.');
    if(!d.tok)   hints.push('Нет данных с датчика температуры: DS18B20 DATA→15 (+резистор 4,7к на 3,3В), либо NTC→34.');
    if(!d.fsok)  hints.push('Память мелодий (LittleFS) недоступна: свои мелодии не загрузить, встроенные работают. Прошивайте со схемой разделов по умолчанию (Default 4MB with spiffs).');
    const dh=byId('dgHint'), hh=hints.length?hints.map(h=>'⚠ '+h).join('<br><br>'):'✅ Все основные модули в норме.';
    if(dh&&dh._h!==hh){ dh._h=hh; dh.innerHTML=hh; } }

  // в историю (с прореживанием) — только при работе датчиков
  pushHistory(p, d.tok?+d.t:0);
  staleTick();   // свежие данные — баннер «нет связи» сразу убрать
}

/* ══════════ ИНИЦИАЛИЗАЦИЯ ══════════ */
loadHistory(); updateDbInfo();
tgtPaint(pendingTgt); calPaint();
byId('fanCard2').innerHTML=fanCardHtml(2,'Выводной вентилятор (реле)');
fanPaint(2);
sndPaintOut(); sndPaintVol(); melParamLabels(); melRenderLib();
syncTime();                       // ⚡ v6.2: часы мозга — с этого устройства
wsConnect();
// ⚡ v6.3: раз в 3 с — проверка «зомби»-сокета и резервный опрос /status
// (без наложения запросов, тайм-аут 2,5 с: не забивать очередь соединений браузера)
setInterval(()=>{ const now=Date.now();
  if(wsAlive&&now-lastMsg>6000){ wsDrop(); wsConnect(); }                        // мозг молчит 6 с — сокет мёртв
  else if(ws&&ws.readyState===0&&now-wsT>10000){ wsDrop(); wsConnect(); }        // подключение зависло
  if((!wsAlive||now-lastMsg>5000)&&!polling){ polling=true;
    getJson('/status',2500).then(r=>r.json()).then(onStatus).catch(()=>{}).then(()=>{ polling=false; }); }
},3000);
setInterval(staleTick,1000);      // счётчик «данные N с назад»
// перерисовка графика раз в 10 сек если открыт
setInterval(()=>{ if(byId('page-graph').classList.contains('active')) drawCharts(); },10000);
// ⚡ v6.3: вернулись на вкладку/разблокировали телефон — сразу проверить сокет
document.addEventListener('visibilitychange',()=>{ if(document.hidden) return;
  if(wsAlive&&Date.now()-lastMsg>6000){ wsDrop(); wsConnect(); } staleTick(); });
// ⚡ v6.3: поворот экрана — перерисовать графики под новую ширину
window.addEventListener('resize',()=>{ clearTimeout(rszT);
  rszT=setTimeout(()=>{ if(byId('page-graph').classList.contains('active')) drawCharts(); },250); });
</script>
</body></html>
)rawliteral";
