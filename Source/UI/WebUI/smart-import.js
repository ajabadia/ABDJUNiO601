/**
 * ABD JUNiO 601 - Smart Import Dialog
 * Multi-format import (tape, sysex, csv, json) dialog logic.
 */

let smartImportData = null;
let selectedDecoderIdx = 0;

listenEvent("onSmartImportProgress", (msg) => {
    const log = document.getElementById('si-progress-log');
    if (!log) return;
    const line = document.createElement('div');
    let color = '#aaa';
    if (msg.startsWith('===')) { color = '#fa0'; line.style.fontWeight = 'bold'; line.style.marginTop = '4px'; }
    else if (msg.startsWith('  [AUTO]')) { color = '#0f0'; line.style.fontWeight = 'bold'; }
    else if (msg.startsWith('  [OPCIONES]')) { color = '#ff0'; line.style.fontWeight = 'bold'; }
    else if (msg.startsWith('    ->')) { color = '#0af'; }
    else if (msg.startsWith('  SNR') || msg.startsWith('  Jitter') || msg.startsWith('  Dropouts') || msg.startsWith('  Duracion') || msg.startsWith('  Calidad')) { color = '#0f0'; }
    else if (msg.includes('ERROR') || msg.includes('Error')) { color = '#f44'; }
    else { color = '#888'; }
    line.style.color = color;
    line.innerText = '> ' + msg;
    log.appendChild(line);
    log.scrollTop = log.scrollHeight;
    const status = document.getElementById('si-progress-status');
    if (status) {
        if (msg.includes('COMPLETADO')) { status.innerText = 'DONE'; status.classList.add('done'); }
        else if (msg.startsWith('===')) { status.innerText = msg.replace(/=/g, '').trim(); status.classList.remove('done'); }
        else { status.innerText = 'PROCESSING...'; status.classList.remove('done'); }
    }
});

listenEvent("onSmartImportResult", (data) => {
    if (!data) return;
    if (data.format === undefined) data.format = 'tape';
    processSmartImportResult(data);
});

listenEvent("onImportResult", (data) => {
    if (!data) return;
    const success = data.success === true;
    const message = data.message || (success ? 'Import completed.' : 'Import failed.');
    showNotification(message, success ? 'success' : 'error');
});

function processSmartImportResult(data) {
    if (!data) return;
    smartImportData = data;

    const status = document.getElementById('si-progress-status');
    if (status) {
        if (data.success) { status.innerText = 'COMPLETED'; status.style.color = '#0f0'; status.classList.add('done'); }
        else { status.innerText = 'FAILED'; status.style.color = '#f44'; status.classList.add('done'); }
    }

    const log = document.getElementById('si-progress-log');
    if (log) {
        const line = document.createElement('div');
        line.style.color = data.success ? '#0f0' : '#f44';
        line.style.fontWeight = 'bold';
        line.innerText = '> ' + (data.success ? 'Analysis complete!' : 'Analysis failed.');
        log.appendChild(line);
        log.scrollTop = log.scrollHeight;
    }

    const fmt = data.format || 'tape';
    const badge = document.getElementById('si-format-badge');
    if (badge) {
        badge.innerText = fmt.toUpperCase();
        const colors = { tape: '#fa0', sysex: '#0af', csv: '#0f0', json: '#f0a', tal: '#a0f' };
        badge.style.background = colors[fmt] || '#fa0';
    }

    const fnEl = document.getElementById('si-fileName');
    if (fnEl && data.fileName) fnEl.innerText = data.fileName;

    const summary = document.getElementById('si-preset-summary');
    if (summary) summary.style.display = 'block';
    if (data.totalPatches !== undefined) {
        document.getElementById('si-preset-count').innerText = data.totalPatches;
        document.getElementById('si-bank-count').innerText = data.banksNeeded || 1;
        document.getElementById('si-import-type').innerText = data.isSinglePatch ? 'Single' : (data.banksNeeded > 1 ? 'Multi-Bank' : 'Full Bank');
        document.getElementById('si-import-format').innerText = data.detailedFormat || fmt.toUpperCase();
    }

    const namesSection = document.getElementById('si-preset-names-section');
    const namesEl = document.getElementById('si-preset-names');
    if (namesSection && namesEl && data.presetNames && data.presetNames.length > 0) {
        namesSection.style.display = 'block';
        document.getElementById('si-names-count').innerText = data.presetNames.length + ' patches';
        namesEl.innerHTML = data.presetNames.map((n, i) =>
            `<span style="color: ${i % 2 === 0 ? '#ccc' : '#888'}">${String(i + 1).padStart(2, '0')}. ${n}</span>`
        ).join('<br>');
    }

    // ── Tape-specific ──
    if (fmt === 'tape') {
        const tapeSection = document.getElementById('si-tape-section');
        if (tapeSection) tapeSection.style.display = 'block';
        if (data.snrDb !== undefined) {
            document.getElementById('si-snr').innerText = data.snrDb.toFixed(1) + ' dB';
            const snrBadge = document.getElementById('si-snr-badge');
            if (snrBadge) {
                if (data.snrDb > 30) { snrBadge.innerText = 'GOOD'; snrBadge.style.background = '#0f0'; }
                else if (data.snrDb > 15) { snrBadge.innerText = 'FAIR'; snrBadge.style.background = '#fa0'; }
                else if (data.snrDb > 8) { snrBadge.innerText = 'POOR'; snrBadge.style.background = '#f80'; }
                else { snrBadge.innerText = 'BAD'; snrBadge.style.background = '#f44'; }
            }
        }
        if (data.jitterPct !== undefined) {
            document.getElementById('si-jitter').innerText = data.jitterPct.toFixed(1) + '%';
            const jBadge = document.getElementById('si-jitter-badge');
            if (jBadge) {
                if (data.jitterPct < 3) { jBadge.innerText = 'GOOD'; jBadge.style.background = '#0f0'; }
                else if (data.jitterPct < 8) { jBadge.innerText = 'FAIR'; jBadge.style.background = '#fa0'; }
                else { jBadge.innerText = 'BAD'; jBadge.style.background = '#f44'; }
            }
        }
        if (data.dropoutPct !== undefined) {
            document.getElementById('si-dropouts').innerText = data.dropoutPct.toFixed(1) + '%';
            const dBadge = document.getElementById('si-dropouts-badge');
            if (dBadge) {
                if (data.dropoutPct < 5) { dBadge.innerText = 'GOOD'; dBadge.style.background = '#0f0'; }
                else if (data.dropoutPct < 15) { dBadge.innerText = 'FAIR'; dBadge.style.background = '#fa0'; }
                else { dBadge.innerText = 'BAD'; dBadge.style.background = '#f44'; }
            }
        }
        if (data.durationS !== undefined) document.getElementById('si-duration').innerText = data.durationS.toFixed(1) + 's';
        if (data.qualityLabel) {
            const qBadge = document.getElementById('si-quality-badge');
            if (qBadge) {
                qBadge.innerText = data.qualityLabel;
                if (data.qualityLabel === 'GOOD') qBadge.style.background = '#0f0';
                else if (data.qualityLabel === 'FAIR') qBadge.style.background = '#fa0';
                else if (data.qualityLabel === 'POOR') qBadge.style.background = '#f80';
                else qBadge.style.background = '#f44';
            }
        }
        const list = document.getElementById('si-decoder-list');
        if (list && data.decoderResults) {
            list.innerHTML = '';
            data.decoderResults.forEach((entry, idx) => {
                const isWinner = (idx === data.winnerIndex);
                const isAuto = data.autoSelected && isWinner;
                const card = document.createElement('div');
                card.className = 'si-decoder-card' + (isWinner ? ' winner' : '');
                card.setAttribute('data-index', idx);
                card.onclick = () => selectSmartDecoder(idx);
                card.innerHTML = `
                    <div class="si-decoder-rank">#${idx + 1}</div>
                    <div class="si-decoder-info">
                        <div class="si-decoder-label">${entry.label} ${isAuto ? '<span style="color:#0f0;font-size:8px;">[AUTO]</span>' : ''}</div>
                        <div class="si-decoder-meta">${entry.patchCount} ${entry.patchCount === 1 ? 'patch' : 'patches'}${entry.duplicates > 0 ? ' (' + entry.duplicates + ' dup)' : ''} | ${entry.elapsedS ? entry.elapsedS.toFixed(1) + 's' : ''} | bytes: ${entry.rawBytes}</div>
                    </div>
                    <div class="si-decoder-check">${isWinner ? '&#10003;' : ''}</div>
                `;
                list.appendChild(card);
            });
        }
        renderSmartWaveform(data.waveform);
    }

    // ── Sysex-specific ──
    if (fmt === 'sysex') {
        const sysexSection = document.getElementById('si-sysex-section');
        if (sysexSection) sysexSection.style.display = 'block';
        if (data.deviceId !== undefined) document.getElementById('si-sysex-device').innerText = '0x' + data.deviceId.toString(16).toUpperCase().padStart(2, '0');
        if (data.functionCode !== undefined) {
            const fns = { 1: 'Bulk Dump (64 patches)', 3: 'Single Patch Dump', 0x10: 'Parameter Change', 0x11: 'Request Dump' };
            document.getElementById('si-sysex-function').innerText = fns[data.functionCode] || '0x' + data.functionCode.toString(16).toUpperCase().padStart(2, '0');
        }
        if (data.checksumValid !== undefined) {
            const el = document.getElementById('si-sysex-checksum');
            el.innerText = data.checksumValid ? 'VALID' : 'INVALID/BYPASS';
            el.style.color = data.checksumValid ? '#0f0' : '#fa0';
        }
        if (data.hexPreview) document.getElementById('si-sysex-hex').innerText = data.hexPreview;
    }

    // ── JSON-specific ──
    if (fmt === 'json') {
        const jsonSection = document.getElementById('si-csv-section');
        if (jsonSection) jsonSection.style.display = 'block';
        if (data.libraryName) document.getElementById('si-csv-columns').innerHTML = `<span style="color: #0af;">${data.libraryName}</span>`;
        if (data.category) { const catEl = document.getElementById('si-csv-params'); if (catEl) catEl.innerText = 'Category: ' + data.category; }
        if (data.columnNames && data.columnNames.length > 0) document.getElementById('si-csv-column-list').innerText = 'JSON Bank: ' + data.totalPatches + ' patches';
    }

    // ── CSV-specific ──
    if (fmt === 'csv') {
        const csvSection = document.getElementById('si-csv-section');
        if (csvSection) csvSection.style.display = 'block';
        if (data.columnCount !== undefined) document.getElementById('si-csv-columns').innerText = data.columnCount;
        if (data.columnNames && data.columnNames.length > 0) {
            document.getElementById('si-csv-params').innerText = data.columnNames.length + ' params';
            document.getElementById('si-csv-column-list').innerText = data.columnNames.join(', ');
        }
    }

    const resultsSec = document.getElementById('si-results-section');
    if (resultsSec) resultsSec.style.display = 'block';

    const importBtn = document.getElementById('btn-si-import');
    if (importBtn) {
        if (data.totalPatches && data.totalPatches > 0) {
            importBtn.disabled = false;
            importBtn.innerHTML = 'IMPORT TO NEXT FREE BANK';
        } else {
            importBtn.disabled = true;
            importBtn.innerHTML = 'NO PATCHES FOUND';
        }
    }

    const modal = document.getElementById('modal-smartImport');
    if (modal) modal.style.display = 'flex';
}

function selectSmartDecoder(idx) {
    selectedDecoderIdx = idx;
    document.querySelectorAll('.si-decoder-card').forEach((card, i) => {
        card.classList.toggle('winner', i === idx);
        const check = card.querySelector('.si-decoder-check');
        if (check) check.innerHTML = (i === idx) ? '&#10003;' : '';
    });
}

function renderSmartWaveform(waveform) {
    const canvas = document.getElementById('si-waveform-canvas');
    if (!canvas || !waveform) return;
    const ctx = canvas.getContext('2d');
    const w = canvas.width, h = canvas.height, midY = h / 2;
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = '#0af';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    for (let px = 0; px < w; px++) {
        const idx = Math.min(Math.floor(px * waveform.length / w), waveform.length - 1);
        if (idx >= waveform.length) break;
        const y = midY - waveform[idx] * midY * 0.9;
        if (px === 0) ctx.moveTo(px, y); else ctx.lineTo(px, y);
    }
    ctx.stroke();
    ctx.strokeStyle = '#333'; ctx.lineWidth = 0.5; ctx.beginPath(); ctx.moveTo(0, midY); ctx.lineTo(w, midY); ctx.stroke();
}

function closeSmartImport() {
    const modal = document.getElementById('modal-smartImport');
    if (modal) modal.style.display = 'none';
    smartImportData = null;
    const log = document.getElementById('si-progress-log');
    if (log) log.innerHTML = '<div style="color: #555;">Waiting for analysis...</div>';
    const resultsSec = document.getElementById('si-results-section');
    if (resultsSec) resultsSec.style.display = 'none';
    const sections = ['si-preset-summary', 'si-preset-names-section', 'si-tape-section', 'si-sysex-section', 'si-csv-section'];
    sections.forEach(id => { const el = document.getElementById(id); if (el) el.style.display = 'none'; });
    const status = document.getElementById('si-progress-status');
    if (status) { status.innerText = 'ANALYZING...'; status.style.color = '#fa0'; status.classList.remove('done'); }
}

function confirmSmartImport() {
    if (!smartImportData) return;
    const fmt = smartImportData.format || 'tape';
    const doImport = () => {
        if (fmt === 'tape') return juce.confirmTapeImport(selectedDecoderIdx);
        else return juce.confirmImportFile();
    };
    const promise = doImport();
    closeSmartImport();
    // Await the native result and show a notification if C++ didn't dispatch one
    if (promise && typeof promise.then === 'function') {
        promise.then(result => {
            if (result && typeof result === 'object' && result.success !== undefined) {
                const msg = result.message || (result.success ? 'Import completed.' : 'Import failed.');
                showNotification(msg, result.success ? 'success' : 'error');
            }
        }).catch(err => {
            console.error('Import error:', err);
        });
    }
}
