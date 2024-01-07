console.log('test');

function setupWS() {
    return new WebSocket('ws://' + location.hostname + ':8080/', ['arduino'])
}

class WSA {
    wsc;
    constructor() {
        this.wsc = setupWS();

        this.wsc.onopen = () => {
            console.log('connected');
        };
        this.wsc.onerror = (error) => {
            console.log('WebSocket Error ', error);
            this.wsc = setupWS();
        };
        this.wsc.onclose = () => {
            console.log('closed');
        };
        this.wsc.onmessage = (e) => {
            console.log('Server: ', e.data);
            if (e.data && e.data === ' ') {
                return;
            }
            try {
                let o = JSON.parse(e.data);
                console.log(o);
                if (o.drv) {
                    setDisabled('.pb', false);
                    setDisabled('.mb', false);

                    Object.entries(o).map(([k, v]) => {
                        switch (k) {
                            case 's':
                                $('#slider', (s) => {
                                    s.value = v.toString();
                                });

                                $('.speed', (s) => {
                                    s.innerHTML = v.toString();
                                });
                                break;
                            case 'ss':
                                $('#motor', (m) => {
                                    m.classList.remove('standstill' , 'moving', 'inactive');
                                    const status = v ? 'standstill' : 'moving' ;
                                    m.innerHTML = status;
                                    m.classList.add(status);

                                    $('.mb', (mb) => {
                                        mb.classList.remove('moving');
                                        if (o.d === -1 && !v) {
                                            mb.classList.add('moving');
                                        }
                                    });
                                    $('.pb', (mb) => {
                                        mb.classList.remove('moving');
                                        if (o.d === 1 && !v) {
                                            mb.classList.add('moving');
                                        }
                                    });
                                });
                                break;
                            case 'scm':
                                $(`.${k}`, (fc) => {
                                    fc.innerHTML = v ? 'StealthChop mode' : 'SpreadCycle mode';
                                });
                                break;
                            default:
                                $(`.${k}`, (fc) => {
                                    if (k.startsWith('ot')) {
                                        if (v) {
                                            fc.classList.remove('hidden');
                                        } else {
                                            fc.classList.add('hidden');
                                        }
                                    } else {
                                        fc.innerHTML = v;
                                    }
                                });
                        }
                    });

                } else {
                    setDisabled('.pb', true);
                    setDisabled('.mb', true);
                    $('#motor', (m) => {
                        m.classList.remove('standstill' , 'moving', 'inactive');
                        m.innerHTML = 'not available';
                        m.classList.add('inactive');
                    })
                }
            } catch (e) {
                console.log(e)
            }
        };

        setInterval(() => {
            if (this.wsc.readyState !== WebSocket.OPEN) {
                this.wsc = setupWS();
            }
        }, 10000);
    }

    so(obj1) {
        if (this.wsc && this.wsc.readyState === WebSocket.OPEN) {
            this.wsc.send(JSON.stringify(obj1));
        } else {
            console.log('No active connection. Dropping', obj1);
        }
    }
}

function move(d, t) {
    fetch(`api/${d}?t=${t}`, {method: 'POST', body: JSON.stringify({})}).then();
}

/**
 *
 * @param {string} selector
 * @param {function(Element)} [callback]
 * @returns {Element|null}
 */
function $(selector, callback) {
    const el = document.querySelector(selector);
    if (el && callback) {
        callback(el);
    }
    return el;
}

function show(q) {
    let el = $(q);
    if (el) {
        el.style.display = 'block';
    }
}

function setDisabled(q, attr) {
    $(q, (el) => {
        if (attr) {
            el.setAttribute('disabled', true);
        } else {
            el.removeAttribute('disabled');
        }
    });
}


function startApplication() {
    let wsc = new WSA();
    let s = 35000;

    const stop = () => {
        wsc.so({"d": 1, "p": false, s});
    };

    $('.slider', (slider) => {
        s = parseInt(slider.value);
        slider.addEventListener('change', (e) => {
            s = parseInt(e.target.value);
            stop();

            $('.speed', (se) => {
                se.innerHTML = s.toString();
            });

        });
    });


    const move = (d) => {
        wsc.so({d, "p": true, s});
    };

    $(".pb",  (pb) => {

        pb.addEventListener("mousedown", () => {
            move(1);
        });

        pb.addEventListener("mouseup", stop);

        pb.addEventListener("touchstart", (e) => {
            e.preventDefault();
            move(1);
        });

        pb.addEventListener("touchend", stop);
    });

    $(".mb", (mb) => {
        mb.addEventListener("mousedown", () => {
            move(-1);
        });
        mb.addEventListener("mouseup", stop);

        mb.addEventListener("touchstart", (e) => {
            e.preventDefault();
            move(-1);
        });
        mb.addEventListener("touchend", stop);
    });

    window.onunload = () => {
        stop();
    };
}

window.addEventListener('load', startApplication);

