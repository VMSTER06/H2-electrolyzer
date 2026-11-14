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
