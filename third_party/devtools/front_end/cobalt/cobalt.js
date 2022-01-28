
/**
 * @implements {SDK.TargetManager.Observer}
 * @unrestricted
 */
export default class CobaltPanel extends UI.VBox {
    constructor() {
        super(true, false);
        SDK.targetManager.observeTargets(this);
        this._target = UI.context.flavor(SDK.Target);
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
