
/**
 * @implements {SDK.TargetManager.Observer}
 * @unrestricted
 */
export default class CobaltPanel extends UI.VBox {
    constructor() {
        const trace_files = [
            ['Trace', 'console_trace.json'],
            ['Timed Trace', 'timed_trace.json']
        ];
        super(true, false);
        SDK.targetManager.observeTargets(this);

        this._target = UI.context.flavor(SDK.Target);
        this._runtimeAgent = this._target.runtimeAgent();

        this.element = this._shadowRoot.createChild('div');
        this.element.textContent = 'Cobalt Console';
        let download_element = this._shadowRoot.createChild('a', 'download');
        download_element.style.display = 'none';

        const traceContainer = this.element.createChild('div', 'trace-container');
        traceContainer.appendChild(UI.createTextButton(Common.UIString('Start Trace'), event => {
            console.log("Start Trace");
            const filename = trace_files[0][1];
            this.run(`(function() { window.h5vcc.traceEvent.start('${filename}');})()`);
            console.log("Started Trace");
        }));
        traceContainer.appendChild(UI.createTextButton(Common.UIString('Stop Trace'), event => {
            console.log("Stop Trace");
            this.run(`(function() { window.h5vcc.traceEvent.stop();})()`);
            console.log("Stopped Trace");
        }));
        trace_files.forEach( (file) => {
            traceContainer.appendChild(UI.createTextButton(Common.UIString('Download ' + file[0]), event => {
                console.log("Download Trace");
                const filename = file[1];
                this.run(`(function() { return window.h5vcc.traceEvent.read('${filename}');})()`).then(function (result) {
                    download_element.setAttribute('href', 'data:text/plain;charset=utf-8,' +
                        encodeURIComponent(result.result.value));
                    download_element.setAttribute('download', filename);
                    console.log("Downloaded Trace");
                    download_element.click();
                    download_element.setAttribute('href', undefined);
                });
            }));
        });
    }

    async run(expression) {
        return await this._runtimeAgent.invoke_evaluate({ expression, returnByValue: true });
    }


    /**
     * @override
     */
    focus() {
    }

    /**
     * @override
     */
    wasShown() {
        super.wasShown();
        if (this._model) {
            this._model.enable();
        }
    }

    /**
     * @override
     */
    willHide() {
        if (this._model) {
            this._model.disable();
        }
        super.willHide();
    }

    /**
     * @override
     * @param {!SDK.Target} target
     */
    targetAdded(target) {
        if (this._model) {
            return;
        }
        if (!this._model) {
            return;
        }
    }

    /**
     * @override
     * @param {!SDK.Target} target
     */
    targetRemoved(target) {
        if (!this._model || this._model.target() !== target) {
            return;
        }
    }
}

/* Legacy exported object */
self.Cobalt = self.Cobalt || {};

/**
 * @constructor
 */
self.Cobalt.CobaltPanel = CobaltPanel;
