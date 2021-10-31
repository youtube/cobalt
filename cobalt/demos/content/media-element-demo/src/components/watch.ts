import {isCobalt} from '../utils/shared_values';
import {Component, renderComponent} from './component';
import {Player} from './player';

/** TODO: Add correct watch props. */
interface Props {}

/** The watch page. */
export class Watch extends Component<Props> {
  /** @override */
  render() {
    return '<div class="player"></div>';
  }

  /** @override */
  afterRender() {
    // Mainstream browsers require users to interact with the document first
    // before it can start play any video. This rule does not apply to Cobalt.
    if (isCobalt) {
      this.boostrapPage();
    } else {
      this.waitForInteraction();
    }
  }

  /**
   * Listens to any key event and displays a message. Bootstraps the page once
   * user interacts with the docuemnt.
   */
  private waitForInteraction() {
    const messageEl = document.createElement('h1');
    messageEl.textContent = 'Press ANY KEY to start';
    this.el.appendChild(messageEl);
    const handler = () => {
      window.removeEventListener('keyup', handler);
      messageEl.parentElement!.removeChild(messageEl);
      this.boostrapPage();
    };
    window.addEventListener('keyup', handler);
  }

  private boostrapPage() {
    renderComponent(Player, this.props, this.el.querySelector('.player')!);
  }
}
